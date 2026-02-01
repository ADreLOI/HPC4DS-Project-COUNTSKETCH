/**
 * COUNT SKETCH - MPI 
 * 
 * This is the main entry point for the MPI-parallelized Count Sketch.
 * 
 * mpirun -np <P> ./app_mpi <input_file> [--benchmark] [--output <file>]
 * 
 * Arguments:
 *   <input_file>   : Path to input data file (one item per line)
 *   --benchmark    : Enable timing output
 *   --output <file>: Write frequency estimates to file
 * 
 * PARALLELIZATION STRATEGY (Scatter + Reduce):
 * 
 * 1. INITIALIZATION
 *    - Rank 0 reads all items from file
 *    - Rank 0 generates hash seeds
 * 
 * 2. BROADCAST SEEDS (MPI_Bcast)
 *    - All ranks need IDENTICAL hash functions
 *    - Rank 0 broadcasts seeds to all ranks
 * 
 * 3. SCATTER DATA (MPI_Scatterv)  
 *    - Rank 0 distributes N/P items to each rank
 *    - Uses Scatterv to handle non-even division
 *    - Each rank gets its chunk of data
 * 
 * 4. LOCAL COMPUTATION
 *    - Each rank builds a local Count Sketch
 *    - Processes only its assigned items
 * 
 * 5. REDUCE TABLES (MPI_Reduce with MPI_SUM)
 *    - All partial tables are summed at rank 0
 *    - Result is the complete Count Sketch!
 * 
 * 6. ESTIMATION (Rank 0 only)
 *    - Rank 0 uses merged table to estimate frequencies
 *  */

#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "../include/count_sketch.h"
#include "../include/count_sketch_mpi.h"

// CONFIGURATION CONSTANTS

/* Count Sketch parameters 
   TUNING GUIDE:
   - DEPTH (d): More rows = better accuracy, but more memory. 7-11 is typical.
   - WIDTH (w): More buckets = fewer collisions. Rule of thumb: w ≈ e/ε² 
     where ε is the desired error rate. For 1M items, use at least 2048-8192.
   
   Possible collisions: If width is too small, estimates will be bad.
   For 1M items with w=64: ~15,000 items per bucket (BAD)
   For 1M items with w=4096: ~244 items per bucket (GOOD)
*/
#define CS_DEPTH 9       // Rows - controls accuracy (odd number for clean median)
#define CS_WIDTH 4096    // Buckets per row - MUST be large enough for your data!

/* Data format */
#define MAX_ITEM_LEN 256  // Maximum length of each item string

/* Files */
#define DEFAULT_OUTPUT "results/estimates.txt"
#define BENCHMARK_CSV  "results/benchmark_results.csv"


//FUNCTION: read_all_items

/* 
   Reads all items from file into a contiguous character array.

   Each item is padded/truncated to MAX_ITEM_LEN bytes for uniform sizing.
   This is done because MPI_Scatterv needs to know exactly how many bytes 
   each rank receives. With variable-length strings, this is complex. 
   By padding to fixed size, we can easily calculate: bytes_for_rank = items_for_rank * MAX_ITEM_LEN
   
   Parameters:
   filename   : Path to input file
   count      : OUTPUT - number of items read
   
   Returns:
   Pointer to contiguous array: [item0][item1][item2]...
   where each [itemN] is MAX_ITEM_LEN bytes
   Returns NULL on error
*/

char* read_all_items(const char *filename, int *count) 
{
    FILE *file = fopen(filename, "r");
    if (!file) {
        fprintf(stderr, "ERROR: Cannot open file '%s'\n", filename);
        return NULL;
    }
    
    // First pass: count lines to allocate exact memory 
    int n = 0;
    char buffer[MAX_ITEM_LEN];
    while (fgets(buffer, sizeof(buffer), file)) {
        n++;
    }
    
    if (n == 0) {
        fprintf(stderr, "ERROR: File '%s' is empty\n", filename);
        fclose(file);
        return NULL;
    }
    
    // Allocate contiguous array: n items × MAX_ITEM_LEN bytes each 
    char *items = (char *)calloc(n * MAX_ITEM_LEN, sizeof(char));
    if (!items) {
        fprintf(stderr, "ERROR: Cannot allocate memory for %d items\n", n);
        fclose(file);
        return NULL;
    }
    
    // Second pass: read items into array 
    rewind(file);
    int i = 0;
    while (fgets(buffer, sizeof(buffer), file) && i < n) {
        // Remove trailing newline 
        buffer[strcspn(buffer, "\n\r")] = '\0';
        
        // Copy to fixed-width slot (strncpy pads with zeros) 
        strncpy(&items[i * MAX_ITEM_LEN], buffer, MAX_ITEM_LEN - 1);
        items[(i + 1) * MAX_ITEM_LEN - 1] = '\0';  // Ensure null termination 
        i++;
    }
    
    fclose(file);
    *count = n;
    
    printf("Read %d items from '%s'\n", n, filename);
    return items;
}

// FUNCTION: calculate_distribution
/* 
   Calculates how to distribute N items among P processes using MPI_Scatterv.
   
   HANDLES NON-EVEN DIVISION:
   --------------------------
   If N=100 and P=3:
   - Base count = 100/3 = 33
   - Remainder = 100%3 = 1
   - Rank 0: 34 items (gets the extra 1)
   - Rank 1: 33 items
   - Rank 2: 33 items
 
   Parameters:
   n_items     : Total number of items
   size        : Number of MPI processes
   sendcounts  : OUTPUT array[size] - items per process
   displs      : OUTPUT array[size] - starting offset for each process
 */
void calculate_distribution(int n_items, int size, int *sendcounts, int *displs) 
{
    int base_count = n_items / size;      // Minimum items per process
    int remainder = n_items % size;       // Extra items to distribute
    
    int offset = 0;
    for (int i = 0; i < size; i++) 
    {
        // First 'remainder' ranks get one extra item
        sendcounts[i] = base_count + (i < remainder ? 1 : 0);
        
        // Displacement is cumulative offset
        displs[i] = offset;
        
        // Update offset for next rank
        offset += sendcounts[i];
    }
    
    // Debug output
    printf("Data distribution across %d processes:\n", size);
    for (int i = 0; i < size; i++) {
        printf("  Rank %d: %d items (offset %d)\n", i, sendcounts[i], displs[i]);
    }
}

// MAIN FUNCTION

int main(int argc, char** argv) 
{
    // STEP 0: Parse command-line arguments
    if (argc < 2) {
        fprintf(stderr, "Usage: mpirun -np <P> %s <input_file> [--benchmark]\n", argv[0]);
        return EXIT_FAILURE;
    }
    
    const char *input_file = argv[1];
    int benchmark_mode = 0;
    const char *output_file = DEFAULT_OUTPUT;
    
    // Parse optional arguments
    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "--benchmark") == 0) {
            benchmark_mode = 1;
        } else if (strcmp(argv[i], "--output") == 0 && i + 1 < argc) {
            output_file = argv[++i];
        }
    }
    
    // STEP 1: Initialize MPI
    MPI_Init(&argc, &argv);
    
    int world_rank, world_size;
    MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);
    MPI_Comm_size(MPI_COMM_WORLD, &world_size);
    
    // Print process info -- debugging
    char processor_name[MPI_MAX_PROCESSOR_NAME];
    int name_len;
    MPI_Get_processor_name(processor_name, &name_len);
    printf("MPI Process %d of %d on %s\n", world_rank, world_size, processor_name);
    
    // Initialize timing struct
    BenchmarkTimings timings = {0};
    double t_start, t_end;
    double time_total_start = MPI_Wtime();
    
    // STEP 2: Rank 0 reads data and generates seeds
    char *all_items = NULL;    // Only rank 0 allocates this
    int n_items = 0;
    uint32_t *seeds = (uint32_t *)malloc(CS_DEPTH * sizeof(uint32_t));
    
    if (world_rank == 0) {
        t_start = MPI_Wtime();
        
        // Read all items from file
        all_items = read_all_items(input_file, &n_items);
        if (!all_items) {
            fprintf(stderr, "ERROR: Failed to read input file\n");
            MPI_Abort(MPI_COMM_WORLD, 1);
        }
        
        // Generate random seeds for hash functions
        // Using fixed seed for reproducibility in benchmarks
        srand(42);
        for (int i = 0; i < CS_DEPTH; i++) {
            seeds[i] = rand();
        }
        
        t_end = MPI_Wtime();
        timings.time_read = t_end - t_start;
        printf("Rank 0: Read %d items in %.6f seconds\n", n_items, timings.time_read);
    }
    
    // STEP 3: Broadcast metadata and seeds to all ranks
    t_start = MPI_Wtime();
    
    // Broadcast number of items (all ranks need this to calculate their share)
    MPI_Bcast(&n_items, 1, MPI_INT, 0, MPI_COMM_WORLD);
    
    // Broadcast seeds (all ranks need identical hash functions!)
    MPI_Bcast(seeds, CS_DEPTH, MPI_UNSIGNED, 0, MPI_COMM_WORLD);
    
    t_end = MPI_Wtime();
    timings.time_bcast = t_end - t_start;
    
    // STEP 4: Calculate data distribution for MPI_Scatterv

    /*
      sendcounts[i] = number of ITEMS for rank i
      displs[i]     = starting ITEM index for rank i
     
      For MPI_Scatterv, we need to convert to BYTES:
      byte_sendcounts[i] = sendcounts[i] * MAX_ITEM_LEN
      byte_displs[i]     = displs[i] * MAX_ITEM_LEN
     */
    int *sendcounts = (int *)malloc(world_size * sizeof(int));
    int *displs = (int *)malloc(world_size * sizeof(int));
    int *byte_sendcounts = (int *)malloc(world_size * sizeof(int));
    int *byte_displs = (int *)malloc(world_size * sizeof(int));
    
    // Calculate item-level distribution
    calculate_distribution(n_items, world_size, sendcounts, displs);
    
    // Convert to byte-level for MPI_Scatterv
    for (int i = 0; i < world_size; i++) {
        byte_sendcounts[i] = sendcounts[i] * MAX_ITEM_LEN;
        byte_displs[i] = displs[i] * MAX_ITEM_LEN;
    }
    
    // Each rank allocates buffer for its chunk 
    int my_item_count = sendcounts[world_rank];
    char *my_items = (char *)malloc(my_item_count * MAX_ITEM_LEN * sizeof(char));
    if (!my_items) {
        fprintf(stderr, "Rank %d: ERROR allocating receive buffer\n", world_rank);
        MPI_Abort(MPI_COMM_WORLD, 1);
    }
    
    // STEP 5: Scatter data chunks using MPI_Scatterv
    t_start = MPI_Wtime();
    
    /* 
    MPI_Scatterv signature:
        sendbuf     - buffer containing all data (only significant at root)
        sendcounts  - array of send counts for each rank
        displs      - array of displacements for each rank
        sendtype    - datatype of send buffer elements
        recvbuf     - buffer to receive data
        recvcount   - number of elements to receive
        recvtype    - datatype of receive buffer elements
        root        - rank of sending process
        comm        - communicator
     */
    MPI_Scatterv(
        all_items,                      // Send buffer (only used by rank 0)
        byte_sendcounts,                // Bytes to send to each rank
        byte_displs,                    // Byte offset for each rank
        MPI_CHAR,                       // Sending chars/bytes
        my_items,                       // Receive buffer
        my_item_count * MAX_ITEM_LEN,   // Bytes this rank receives
        MPI_CHAR,                       // Receiving chars/bytes
        0,                              // Root rank (sender)
        MPI_COMM_WORLD
    );
    
    // Synchronize before timing
    MPI_Barrier(MPI_COMM_WORLD);
    t_end = MPI_Wtime();
    timings.time_scatter = t_end - t_start;
    
    printf("Rank %d: Received %d items\n", world_rank, my_item_count);
    
    // Rank 0 can free the full data now
    if (world_rank == 0) {
        free(all_items);
        all_items = NULL;
    }
    
    // STEP 6: Build local Count Sketch from received items
    t_start = MPI_Wtime();
    
    // Create local Count Sketch with shared seeds
    CountSketch *local_cs = cs_create_with_seeds(CS_DEPTH, CS_WIDTH, seeds);
    if (!local_cs) {
        fprintf(stderr, "Rank %d: ERROR creating Count Sketch\n", world_rank);
        MPI_Abort(MPI_COMM_WORLD, 1);
    }
    
    // Process local items
    for (int i = 0; i < my_item_count; i++) 
    {
        char *item = &my_items[i * MAX_ITEM_LEN];
        cs_update(local_cs, item);
    }
        
    // Ensure all ranks finish before timing
    MPI_Barrier(MPI_COMM_WORLD);  
    t_end = MPI_Wtime();
    timings.time_compute = t_end - t_start;
    
    printf("Rank %d: Processed %d items in %.6f seconds\n", 
           world_rank, my_item_count, timings.time_compute);
    
    // STEP 7: Reduce all partial tables at rank 0 using MPI_Reduce
    t_start = MPI_Wtime();
    
    // Flatten local table to 1D for MPI communication
    int table_size = CS_DEPTH * CS_WIDTH;
    int32_t *local_flat = cs_flatten(local_cs);
    
    // Allocate receive buffer at rank 0 only
    int32_t *result_flat = NULL;
    if (world_rank == 0) {
        result_flat = (int32_t *)malloc(table_size * sizeof(int32_t));
        if (!result_flat) {
            fprintf(stderr, "Rank 0: ERROR allocating result buffer\n");
            MPI_Abort(MPI_COMM_WORLD, 1);
        }
    }
    
    /*
     MPI_Reduce signature:
       sendbuf  - buffer containing local data
       recvbuf  - buffer to receive result (only significant at root)
       count    - number of elements
       datatype - datatype of elements
       op       - reduction operation (MPI_SUM combines all values!)
       root     - rank receiving the result
       comm     - communicator
     
     After this call:
       result_flat[i] = Σ local_flat[i] across all ranks. This is because of Count Sketch's additive nature.
     */
    MPI_Reduce(
        local_flat,      // Send buffer (each rank's partial table)
        result_flat,     // Receive buffer (only used at rank 0)
        table_size,      // Number of elements: d × w
        MPI_INT32_T,     // Element type: 32-bit signed integers
        MPI_SUM,         // Operation: SUM all partial tables
        0,               // Root rank (receiver)
        MPI_COMM_WORLD
    );
    
    MPI_Barrier(MPI_COMM_WORLD);
    t_end = MPI_Wtime();
    timings.time_reduce = t_end - t_start;
    
    // Clean up local data
    free(local_flat);
    cs_destroy(local_cs);
    free(my_items);
    
    // STEP 8: Rank 0 estimates frequencies and outputs results
    if (world_rank == 0) {
        t_start = MPI_Wtime();
        
        // Reconstruct Count Sketch from flattened result
        CountSketch *final_cs = cs_unflatten(result_flat, CS_DEPTH, CS_WIDTH, seeds);
        if (!final_cs) {
            fprintf(stderr, "ERROR: Failed to unflatten result\n");
            MPI_Abort(MPI_COMM_WORLD, 1);
        }
        
        // Read items again for frequency estimation
        // (In production, you'd keep unique items or query specific ones)
        int query_count;
        char *query_items = read_all_items(input_file, &query_count);
        
        // Open output file
        FILE *out = fopen(output_file, "w");
        if (!out) {
            fprintf(stderr, "WARNING: Cannot open output file, using stdout\n");
            out = stdout;
        }
        
        // Estimate and output frequencies
        fprintf(out, "# Frequency Estimates (MPI with %d processes)\n", world_size);
        fprintf(out, "# Item, Estimated Frequency\n");
        
        // Process unique items to avoid duplicates in output
        // For simplicity, just show first 100 or all if less
        int max_output = (query_count < 100) ? query_count : 100;
        for (int i = 0; i < max_output; i++) {
            char *item = &query_items[i * MAX_ITEM_LEN];
            int32_t estimate = cs_estimate(final_cs, item);
            fprintf(out, "%s, %d\n", item, estimate);
        }
        
        if (out != stdout) {
            fclose(out);
            printf("Results written to '%s'\n", output_file);
        }
        
        t_end = MPI_Wtime();
        timings.time_estimate = t_end - t_start;
        
        // Clean up
        free(query_items);
        cs_destroy(final_cs);
        free(result_flat);
    }
    
    // STEP 9: Output timing results
    double time_total_end = MPI_Wtime();
    timings.time_total = time_total_end - time_total_start;
    
    if (benchmark_mode) {
        print_timings(&timings, world_rank, world_size, n_items);
        
        if (world_rank == 0) {
            write_timings_csv(BENCHMARK_CSV, &timings, world_size, n_items);
            printf("Benchmark data appended to '%s'\n", BENCHMARK_CSV);
        }
    }
    
    // STEP 10: Cleanup and finalize
    free(seeds);
    free(sendcounts);
    free(displs);
    free(byte_sendcounts);
    free(byte_displs);
    
    MPI_Finalize();
    
    if (world_rank == 0) {
        printf("\n MPI Count Sketch completed successfully \n");
    }
    
    return EXIT_SUCCESS;
}