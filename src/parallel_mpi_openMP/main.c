#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <omp.h>
#include "../include/count_sketch.h"
#include <time.h>
#include <sys/time.h>
#include <string.h>

#define MAX_ITEM_LENGTH 64

int main(int argc, char** argv) 
{
    srand(time(NULL));
    //Initialize the MPI environment
    MPI_Init(&argc, &argv);

    int size;
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    int my_rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &my_rank);

    if(argc < 4) 
    {
        if(my_rank == 0) {
            printf("Usage: mpirun -np <num_processes> %s <num_trials> <num_buckets> <input_file_path>\n", argv[0]);
        }
        MPI_Finalize();
        return 1;
    }

    int depth = atoi(argv[1]);
    int width = atoi(argv[2]);
    char* input_file = argv[3];
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
        
        char item[MAX_ITEM_LENGTH];
        while (fgets(item, sizeof(item), file))
        {
            total_lines++;
        }
        fclose(file);
        printf("Total lines in input file: %d\n", total_lines); //OK

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
    printf("Process %d received chunk size: %d\n", my_rank, chunk_size);

    char *local_data = malloc(chunk_size * MAX_ITEM_LENGTH);
    double wt1,wt2;

    //Each process creates its own Count Sketch
    CountSketch *local_cs = cs_create(depth, width);

    //Bypassing the seed problem (each process must have the same seeds)
    if (my_rank==0)
    {
        for (int i = 0; i < depth; i++) local_cs->seeds[i] = rand();
    }
    MPI_Bcast(local_cs->seeds, depth, MPI_UINT32_T, 0, MPI_COMM_WORLD);

    //Scatter
    MPI_Scatter(global_data, chunk_size * MAX_ITEM_LENGTH, MPI_CHAR,
                local_data, chunk_size * MAX_ITEM_LENGTH, MPI_CHAR,
                0, MPI_COMM_WORLD);


    //Update local sketch in parallel using OpenMP
    omp_set_num_threads(16); //Set number of threads, can be adjusted

    wt1 = MPI_Wtime();
    #pragma omp parallel for schedule(dynamic)
    for(int i = 0; i < chunk_size; i++)
    {
        cs_update(local_cs, &local_data[i * MAX_ITEM_LENGTH]);   
    }
    wt2 = MPI_Wtime();
    // 6. Reduce (Using a contiguous block for the table)
    CountSketch *final_sketch = NULL;
    if (my_rank == 0) 
    {
        final_sketch = cs_create(depth, width);
        memcpy(final_sketch->seeds, local_cs->seeds, depth * sizeof(uint32_t));
    }

    MPI_Reduce(local_cs->table[0], (my_rank == 0) ? final_sketch->table[0] : NULL, 
           depth * width, MPI_INT32_T, MPI_SUM, 0, MPI_COMM_WORLD);

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
        printf("Total time taken: %f seconds\n", wt2 - wt1);
    }

    cs_destroy(local_cs);
    free(local_data);
    MPI_Finalize(); 
    return 0;
}