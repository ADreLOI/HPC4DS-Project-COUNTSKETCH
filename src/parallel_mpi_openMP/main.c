#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <omp.h>
#include "../include/count_sketch.h"
#include <time.h>
#include <sys/time.h>
#include <string.h>

#define MAX_ITEM_LENGTH 64
#define ITERATIONS 10 
float totSerialTime = 0.0;
float totParallelTime = 0.0;
float totCommParallelTime = 0.0;

void ComputeCountSketchHybrid(int depth, int width, CountSketch *local_cs, CountSketch** final_sketch, int my_rank, int size, int chunk_size, char* global_data, int n_threads, char* local_data)
{

    double wt1,wt2;
    double wt_comm1, wt_comm2;
    //Bypassing the seed problem (each process must have the same seeds)
    if (my_rank==0)
    {
        for (int i = 0; i < depth; i++) local_cs->seeds[i] = rand();
    }
    MPI_Bcast(local_cs->seeds, depth, MPI_UINT32_T, 0, MPI_COMM_WORLD);

    wt1 = MPI_Wtime();
    //Scatter
    MPI_Scatter(global_data, chunk_size * MAX_ITEM_LENGTH, MPI_CHAR,
                local_data, chunk_size * MAX_ITEM_LENGTH, MPI_CHAR,
                0, MPI_COMM_WORLD);


    //Update local sketch in parallel using OpenMP
    omp_set_num_threads(omp_get_max_threads()); //Set number of threads, can be adjusted

    wt_comm1 = MPI_Wtime();
    #pragma omp parallel for schedule(dynamic)
    for(int i = 0; i < chunk_size; i++)
    {
        cs_update(local_cs, &local_data[i * MAX_ITEM_LENGTH]);   
    }
    wt_comm2 = MPI_Wtime();
    // 6. Reduce (Using a contiguous block for the table)
    if (my_rank == 0) 
    {
        *final_sketch = cs_create(depth, width);
        memcpy((*final_sketch)->seeds, local_cs->seeds, depth * sizeof(uint32_t));
    }

    MPI_Reduce(local_cs->table[0], (my_rank == 0) ? (*final_sketch)->table[0] : NULL, 
           depth * width, MPI_INT32_T, MPI_SUM, 0, MPI_COMM_WORLD);

    wt2 = MPI_Wtime();

    totParallelTime += (wt2 - wt1);
    totCommParallelTime += (wt_comm2 - wt_comm1);
};

//Function only for comparison purposes
void ComputeSerialCountSketch(int depth, int width, CountSketch *cs, char* data, int total_lines)
{
    double wt1, wt2;
    wt1 = MPI_Wtime();
    for(int i = 0; i < total_lines; i++) 
    {
        cs_update(cs, &data[i * MAX_ITEM_LENGTH]);
    }
    wt2 = MPI_Wtime();
    totSerialTime += (wt2 - wt1);

    cs_destroy(cs);
}


int main(int argc, char** argv) 
{
    srand(time(NULL));
    //Initialize the MPI environment
    MPI_Init(&argc, &argv);

    int size;
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    int my_rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &my_rank);

    if(argc < 5) 
    {
        if(my_rank == 0) {
            printf("Usage: mpirun -np <num_processes> %s <num_trials> <num_buckets> <input_file_path> <num_threads>\n", argv[0]);
        }
        MPI_Finalize();
        return 1;
    }

    int depth = atoi(argv[1]);
    int width = atoi(argv[2]);
    char* input_file = argv[3];
    int n_threads = atoi(argv[4]);
    char* global_data = NULL;
    int total_lines = 0;
    int chunk_size = 0;

    if(my_rank == 0) 
    {
        //Couting the number of lines in the input file (Hoping not too much overhead in big files)
        FILE *file = fopen(input_file, "r");
        if (file == NULL)
        {
            perror("Unable to open file!");
            MPI_Finalize();
            return EXIT_FAILURE;
        }
        
        //Count lines in file to allocate global buffer
        char item[MAX_ITEM_LENGTH];
        while (fgets(item, sizeof(item), file))
        {
            total_lines++;
        }
        fclose(file);
        printf("Total lines in input file: %d\n", total_lines); 

        chunk_size = total_lines / size;

        //Allocates the buffer before scatter
        global_data = (char*)malloc(total_lines* MAX_ITEM_LENGTH * sizeof(char));

        //Read the file again to fill the buffer
        file = fopen(input_file, "r");
        if (file == NULL)
        {
            perror("Unable to open file!");
            free(global_data);
            MPI_Finalize();
            return EXIT_FAILURE;
        }
        int index = 0;
        while (fgets(item, sizeof(item), file))
        {
            // Remove newline character if present
            item[strcspn(item, "\n")] = 0;
            strcpy(&global_data[index * MAX_ITEM_LENGTH], item);
            index++;
        }
        fclose(file);
    }

    //Receive chunk size from root process
    MPI_Bcast(&chunk_size, 1, MPI_INT, 0, MPI_COMM_WORLD); 
    //printf("Process %d received chunk size: %d\n", my_rank, chunk_size);

    char *local_data = malloc(chunk_size * MAX_ITEM_LENGTH);

    //Each processc creates an instance for the final sketch (only rank 0 will use it)
    CountSketch *final_sketch = NULL;

    if(my_rank==0)
    {
        printf("Starting Serial Computation for %d iterations...\n", ITERATIONS);
        for(int iter = 0; iter < ITERATIONS; iter++)
        {
            ComputeSerialCountSketch(depth, width, cs_create(depth, width), global_data, total_lines);
        }
    }

    MPI_Barrier(MPI_COMM_WORLD); //Synchronize before starting parallel computation

    for(int iter = 0; iter < ITERATIONS; iter++)
    {
         //Each process creates its own Count Sketch
        CountSketch *local_cs = cs_create(depth, width);
        free(final_sketch);
        final_sketch = NULL;
        //Compute the Count Sketch iterating in order to attenuate outliers
        ComputeCountSketchHybrid(depth, width, local_cs, &final_sketch, my_rank, size, chunk_size, global_data, n_threads, local_data);
        //Deallocate each time the local sketch
        cs_destroy(local_cs);
    }


    if(my_rank == 0)
    {
        //Estimate frequencies of some items (for demonstration)
        for(int i = 0; i < total_lines; i++) 
        {
            int32_t estimate = cs_estimate(final_sketch, &global_data[i * MAX_ITEM_LENGTH]);
            if (i < 10)
            {
                printf("Estimated frequency of %s: %d\n", &global_data[i * MAX_ITEM_LENGTH], estimate);
            }
        }
        cs_destroy(final_sketch);
        free(global_data);

        //Prints    
        printf("| Processes | N_Threads | Serial Time | Compute Time | Compute + Communication Time | Speedup Compute | Efficiency Compute | Speedup Communication | Efficiency Communication |\n");
        printf("|-----------|-----------|-------------|--------------|------------------------------|-----------------|--------------------|-----------------------|--------------------------|\n");
        float avgSerialTime = totSerialTime / ITERATIONS;
        float avgParallelTime = totParallelTime / ITERATIONS;
        float avgCommTime = totCommParallelTime / ITERATIONS;

        float speedupCompute = avgSerialTime / (avgCommTime);
        float efficiencyCompute = (speedupCompute / size) * 100;
        float speedupComm = avgSerialTime / avgParallelTime;
        float efficiencyComm = (speedupComm / size) * 100;
        printf("| %9d | %9d | %11.6f | %12.6f | %28.6f | %15.6f | %17.2f%% | %21.6f | %23.2f%% |\n",
               size, n_threads, avgSerialTime, avgCommTime, avgParallelTime,
               speedupCompute, efficiencyCompute, speedupComm, efficiencyComm);

        //Save in CSV file
        char csv_path[256];
        snprintf(csv_path, sizeof(csv_path), "results/performance_hybrid_%d.csv", total_lines);
        FILE *csv_file = fopen(csv_path, "a");
        if (csv_file == NULL)
        {
            perror("Unable to open CSV file!");
            MPI_Finalize();
            return EXIT_FAILURE;
        }
        fprintf(csv_file, "%d,%d,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f\n",
                size, n_threads, avgSerialTime, avgCommTime, avgParallelTime,
                speedupCompute, efficiencyCompute, speedupComm, efficiencyComm);
        fclose(csv_file);
    }

    free(local_data);
    MPI_Finalize(); 
    return 0;
}