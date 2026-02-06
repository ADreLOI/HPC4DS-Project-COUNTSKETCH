/**
 * COUNT SKETCH - MPI IMPLEMENTATION
 * 
 * USAGE:
 *   mpirun -np <P> ./app_mpi <trials> <buckets> <input_file> <scaling_mode>
 * 
 * ARGUMENTS:
 *   trials       - Number of iterations for averaging results
 *   buckets      - Width of Count Sketch (number of buckets per row)
 *   input_file   - Path to input data file (one item per line)
 *   scaling_mode - 0 = Strong Scaling, 1 = Weak Scaling
 * 
 * OUTPUT:
 *   Prints table with timing and speedup/efficiency metrics
 *   Saves results to CSV for plotting
 * 
 * PARALLELIZATION STRATEGY (Scatter + Reduce):
 *   1. Rank 0 reads all items from file
 *   2. Rank 0 broadcasts hash seeds to all ranks
 *   3. Data is scattered to all ranks via MPI_Scatterv
 *   4. Each rank builds a local Count Sketch
 *   5. Partial tables are reduced (summed) at rank 0 via MPI_Reduce
 *   6. Rank 0 estimates frequencies
 */

#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "../include/count_sketch.h"
#include "../include/count_sketch_mpi.h"

// Configuration
#define MAX_ITEM_LENGTH 64
#define DEPTH 5              // Number of hash functions (same as Hybrid)
#define ITERATIONS 10        // Internal iterations for timing stability

// Global timing accumulators (matching Hybrid structure)
float totSerialTime = 0.0;
float totParallelTime = 0.0;      // Compute + Communication time
float totCommParallelTime = 0.0;  // Compute only time

/**
 * Read all items from file into contiguous buffer
 */
char* read_all_items(const char *filename, int *count) 
{
    FILE *file = fopen(filename, "r");
    if (!file) {
        fprintf(stderr, "ERROR: Cannot open file '%s'\n", filename);
        return NULL;
    }
    
    // First pass: count lines
    int n = 0;
    char buffer[MAX_ITEM_LENGTH];
    while (fgets(buffer, sizeof(buffer), file)) {
        n++;
    }
    
    if (n == 0) {
        fprintf(stderr, "ERROR: File '%s' is empty\n", filename);
        fclose(file);
        return NULL;
    }
    
    // Allocate contiguous array
    char *items = (char *)calloc(n * MAX_ITEM_LENGTH, sizeof(char));
    if (!items) {
        fprintf(stderr, "ERROR: Cannot allocate memory for %d items\n", n);
        fclose(file);
        return NULL;
    }
    
    // Second pass: read items
    rewind(file);
    int i = 0;
    while (fgets(buffer, sizeof(buffer), file) && i < n) {
        buffer[strcspn(buffer, "\n\r")] = '\0';
        strncpy(&items[i * MAX_ITEM_LENGTH], buffer, MAX_ITEM_LENGTH - 1);
        items[(i + 1) * MAX_ITEM_LENGTH - 1] = '\0';
        i++;
    }
    
    fclose(file);
    *count = n;
    return items;
}

/**
 * Compute Count Sketch serially (for baseline timing)
 */
void ComputeSerialCountSketch(int depth, int width, CountSketch *cs, char* data, int total_lines)
{
    for(int i = 0; i < total_lines; i++) 
    {
        cs_update(cs, &data[i * MAX_ITEM_LENGTH]);
    }
}

/**
 * Compute distribution for MPI_Scatterv
 */
void calculate_distribution(int n_items, int size, int *sendcounts, int *displs)
{
    int base_count = n_items / size;
    int remainder = n_items % size;
    
    int offset = 0;
    for (int i = 0; i < size; i++) {
        sendcounts[i] = base_count + (i < remainder ? 1 : 0);
        displs[i] = offset;
        offset += sendcounts[i];
    }
}

int main(int argc, char** argv) 
{
    srand(time(NULL));
    
    // Initialize MPI
    MPI_Init(&argc, &argv);
    
    int size, my_rank;
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    MPI_Comm_rank(MPI_COMM_WORLD, &my_rank);
    
    // Parse arguments (matching Hybrid format)
    if(argc < 5) {
        if(my_rank == 0) {
            printf("Usage: mpirun -np <num_processes> %s <num_trials> <num_buckets> <input_file> <scaling_mode>\n", argv[0]);
            printf("  scaling_mode: 0 = Strong Scaling, 1 = Weak Scaling\n");
        }
        MPI_Finalize();
        return 1;
    }
    
    int depth = atoi(argv[1]);        // Actually trials for Hybrid, but we use as depth
    int width = atoi(argv[2]);        // Buckets = width
    char* input_file = argv[3];
    int scaling_mode = atoi(argv[4]); // 0 = Strong, 1 = Weak
    
    // For MPI, we don't have threads (show 1 in output)
    int n_threads = 1;
    
    char* global_data = NULL;
    int total_lines = 0;
    int original_total_lines = 0; // Preserve for CSV filename
    int chunk_size = 0;
    
    // Generate seeds (same across all ranks)
    uint32_t seeds[DEPTH];
    if(my_rank == 0) {
        srand(42);  // Fixed seed for reproducibility
        for(int i = 0; i < DEPTH; i++) {
            seeds[i] = rand();
        }
    }
    MPI_Bcast(seeds, DEPTH, MPI_UNSIGNED, 0, MPI_COMM_WORLD);
    
    // Rank 0 reads data
    if(my_rank == 0) {
        FILE *file = fopen(input_file, "r");
        if (file == NULL) {
            perror("Unable to open file!");
            MPI_Finalize();
            return EXIT_FAILURE;
        }
        
        // Count lines
        char item[MAX_ITEM_LENGTH];
        while (fgets(item, sizeof(item), file)) {
            total_lines++;
        }
        fclose(file);
        original_total_lines = total_lines; // Preserve for CSV filename
        printf("Total lines in input file: %d\n", total_lines);
        
        // Weak scaling: adjust total lines
        if(scaling_mode == 1) {
            chunk_size = total_lines / 64;  // Base chunk from 8M/64
            total_lines = chunk_size * size; // Scale with processes
        } else {
            chunk_size = total_lines / size;
        }
        
        // Allocate and read data
        global_data = (char*)malloc(total_lines * MAX_ITEM_LENGTH * sizeof(char));
        file = fopen(input_file, "r");
        if (file == NULL) {
            perror("Unable to open file!");
            free(global_data);
            MPI_Finalize();
            return EXIT_FAILURE;
        }
        
        for(int i = 0; i < total_lines; i++) {
            if(fgets(item, sizeof(item), file) == NULL) break;
            item[strcspn(item, "\n\r")] = '\0';
            strncpy(&global_data[i * MAX_ITEM_LENGTH], item, MAX_ITEM_LENGTH - 1);
            global_data[(i + 1) * MAX_ITEM_LENGTH - 1] = '\0';
        }
        fclose(file);
    }
    
    // Broadcast metadata
    MPI_Bcast(&total_lines, 1, MPI_INT, 0, MPI_COMM_WORLD);
    MPI_Bcast(&chunk_size, 1, MPI_INT, 0, MPI_COMM_WORLD);
    
    // Calculate distribution
    int *sendcounts = (int*)malloc(size * sizeof(int));
    int *displs = (int*)malloc(size * sizeof(int));
    calculate_distribution(total_lines, size, sendcounts, displs);
    
    // Allocate local buffer
    int my_count = sendcounts[my_rank];
    char* local_data = (char*)malloc(my_count * MAX_ITEM_LENGTH * sizeof(char));
    
    // Convert to bytes for MPI_Scatterv
    int *byte_sendcounts = (int*)malloc(size * sizeof(int));
    int *byte_displs = (int*)malloc(size * sizeof(int));
    for(int i = 0; i < size; i++) {
        byte_sendcounts[i] = sendcounts[i] * MAX_ITEM_LENGTH;
        byte_displs[i] = displs[i] * MAX_ITEM_LENGTH;
    }
    
    // Create final sketch pointer (for rank 0)
    CountSketch* final_sketch = NULL;
    
    // Run multiple iterations for averaging
    for(int iter = 0; iter < ITERATIONS; iter++) {
        float serialTime = 0.0, parallelTime = 0.0, commTime = 0.0;
        
        // --- Serial baseline (rank 0 only) ---
        if(my_rank == 0) {
            CountSketch* serial_cs = cs_create_with_seeds(DEPTH, width, seeds);
            
            double t1 = MPI_Wtime();
            ComputeSerialCountSketch(DEPTH, width, serial_cs, global_data, total_lines);
            double t2 = MPI_Wtime();
            
            serialTime = (float)(t2 - t1);
            cs_destroy(serial_cs);
        }
        
        MPI_Barrier(MPI_COMM_WORLD);
        
        // --- Parallel computation ---
        double t_total_start = MPI_Wtime();
        
        // Scatter data
        MPI_Scatterv(global_data, byte_sendcounts, byte_displs, MPI_CHAR,
                     local_data, my_count * MAX_ITEM_LENGTH, MPI_CHAR,
                     0, MPI_COMM_WORLD);
        
        MPI_Barrier(MPI_COMM_WORLD);
        double t_compute_start = MPI_Wtime();
        
        // Build local Count Sketch
        CountSketch* local_cs = cs_create_with_seeds(DEPTH, width, seeds);
        for(int i = 0; i < my_count; i++) {
            cs_update(local_cs, &local_data[i * MAX_ITEM_LENGTH]);
        }
        
        MPI_Barrier(MPI_COMM_WORLD);
        double t_compute_end = MPI_Wtime();
        
        // Reduce tables
        int table_size = DEPTH * width;
        int32_t *local_flat = cs_flatten(local_cs);
        int32_t *result_flat = NULL;
        
        if(my_rank == 0) {
            result_flat = (int32_t*)malloc(table_size * sizeof(int32_t));
        }
        
        MPI_Reduce(local_flat, result_flat, table_size, MPI_INT32_T, MPI_SUM, 0, MPI_COMM_WORLD);
        
        MPI_Barrier(MPI_COMM_WORLD);
        double t_total_end = MPI_Wtime();
        
        // Calculate times
        commTime = (float)(t_compute_end - t_compute_start);  // Compute only
        parallelTime = (float)(t_total_end - t_total_start);  // Total with communication
        
        // Accumulate
        totSerialTime += serialTime;
        totCommParallelTime += commTime;
        totParallelTime += parallelTime;
        
        // Keep final sketch from last iteration
        if(iter == ITERATIONS - 1 && my_rank == 0) {
            final_sketch = cs_unflatten(result_flat, DEPTH, width, seeds);
        }
        
        // Cleanup iteration
        free(local_flat);
        if(my_rank == 0) free(result_flat);
        cs_destroy(local_cs);
    }
    
    // Rank 0 outputs results
    if(my_rank == 0) {
        // Estimate some frequencies (for demonstration)
        if(scaling_mode == 1) {
            total_lines = chunk_size * size;
        }
        for(int i = 0; i < 10 && i < total_lines; i++) {
            int32_t estimate = cs_estimate(final_sketch, &global_data[i * MAX_ITEM_LENGTH]);
            printf("Estimated frequency of %s: %d\n", &global_data[i * MAX_ITEM_LENGTH], estimate);
        }
        cs_destroy(final_sketch);
        free(global_data);
        
        // Calculate averages
        float avgSerialTime = totSerialTime / ITERATIONS;
        float avgParallelTime = totParallelTime / ITERATIONS;
        float avgCommTime = totCommParallelTime / ITERATIONS;
        
        if(scaling_mode == 1) {
            // Weak Scaling output
            float weakScalingCompute = avgSerialTime / avgCommTime;
            float weakScalingComm = avgSerialTime / avgParallelTime;
            
            printf("| Processes | N_Threads | Total Lines | Serial Time | Compute Time | Compute + Communication Time | Weak Scaling Compute | Weak Scaling Communication |\n");
            printf("|-----------|-----------|-------------|-------------|--------------|------------------------------|----------------------|----------------------------|\n");
            printf("| %9d | %9d | %11d | %11.6f | %12.6f | %28.6f | %20.6f | %26.6f |\n",
                   size, n_threads, chunk_size * size, avgSerialTime, avgCommTime, avgParallelTime,
                   weakScalingCompute, weakScalingComm);
            
            // Save to CSV - use original file size for filename
            char csv_path[256];
            snprintf(csv_path, sizeof(csv_path), "results/weak_scaling_mpi_%d.csv", original_total_lines);
            FILE *csv_file = fopen(csv_path, "a");
            if(csv_file) {
                fprintf(csv_file, "%d,%d,%d,%.6f,%.6f,%.6f,%.6f,%.6f\n",
                        size, n_threads, chunk_size * size, avgSerialTime, avgCommTime, avgParallelTime,
                        weakScalingCompute, weakScalingComm);
                fclose(csv_file);
            }
        } else {
            // Strong Scaling output
            float speedupCompute = avgSerialTime / avgCommTime;
            float efficiencyCompute = (speedupCompute / size) * 100;
            float speedupComm = avgSerialTime / avgParallelTime;
            float efficiencyComm = (speedupComm / size) * 100;
            
            printf("| Processes | N_Threads | Serial Time | Compute Time | Compute + Communication Time | Speedup Compute | Efficiency Compute | Speedup Communication | Efficiency Communication |\n");
            printf("|-----------|-----------|-------------|--------------|------------------------------|-----------------|--------------------|-----------------------|--------------------------|\n");
            printf("| %9d | %9d | %11.6f | %12.6f | %28.6f | %15.6f | %17.2f%% | %21.6f | %23.2f%% |\n",
                   size, n_threads, avgSerialTime, avgCommTime, avgParallelTime,
                   speedupCompute, efficiencyCompute, speedupComm, efficiencyComm);
            
            // Save to CSV
            char csv_path[256];
            snprintf(csv_path, sizeof(csv_path), "results/performance_mpi_%d.csv", total_lines);
            FILE *csv_file = fopen(csv_path, "a");
            if(csv_file) {
                fprintf(csv_file, "%d,%d,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f\n",
                        size, n_threads, avgSerialTime, avgCommTime, avgParallelTime,
                        speedupCompute, efficiencyCompute, speedupComm, efficiencyComm);
                fclose(csv_file);
            }
        }
    }
    
    // Cleanup
    free(local_data);
    free(sendcounts);
    free(displs);
    free(byte_sendcounts);
    free(byte_displs);
    
    MPI_Finalize();
    return 0;
}