// COUNT SKETCH - MPI PARALLEL IMPLEMENTATION
 /* 
 This file contains the Count Sketch algorithm adapted for MPI parallelization.
 It includes both the core algorithm (same as sequential) and MPI utilities:
   - cs_create_with_seeds() : Create CS with specific seeds for MPI consistency
   - cs_merge()             : Merge two Count Sketches (for combining partials)
   - cs_flatten()           : Convert 2D table to 1D for MPI communication
   - cs_unflatten()         : Convert 1D array back to 2D table
   - print_timings()        : Display benchmark results
   - write_timings_csv()    : Export timings for analysis
 */

#include "../include/count_sketch.h"
#include "../include/count_sketch_mpi.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// HASH FUNCTION: FNV-1a
/*
 FNV-1a (Fowler-Noll-Vo) is a fast, non-cryptographic hash function.
 
 Properties:
   - Very fast to compute
   - Good distribution for hash tables
   - Deterministic (same input + seed = same output)
 
 The seed parameter allows us to create multiple independent hash functions
 from a single implementation (one per row of the Count Sketch).
 */
static uint32_t hash_fnv1a(const char *key, uint32_t seed) 
{
    // FNV offset basis XORed with seed for variation
    uint32_t hash = 2166136261U ^ seed;
    
    // Process each character
    while (*key) {
        hash ^= (uint8_t)*key++;    // XOR with byte 
        hash *= 16777619U;          // Multiply by FNV prime 
    }
    return hash;
}

// COMPARISON FUNCTION FOR QSORT
/*
 Used by cs_estimate() to find the median of estimates.
 */
static int compare_ints(const void *a, const void *b) 
{
    int32_t arg1 = *(const int32_t *)a;
    int32_t arg2 = *(const int32_t *)b;
    if (arg1 < arg2) return -1;
    if (arg1 > arg2) return 1;
    return 0;
}

// FUNCTION: cs_create
/*
 Creates a new Count Sketch with RANDOM seeds.
 Use this for sequential implementation or when rank 0 initializes.
 */
CountSketch* cs_create(int d, int w) 
{
    CountSketch *cs = (CountSketch *)malloc(sizeof(CountSketch));
    if (!cs) {
        fprintf(stderr, "ERROR: Failed to allocate CountSketch\n");
        return NULL;
    }
    
    cs->d = d;  // Number of hash functions / rows
    cs->w = w;  // Number of buckets per row

    // Allocate 2D table: array of row pointers
    cs->table = (int32_t **)malloc(d * sizeof(int32_t *));
    if (!cs->table) {
        fprintf(stderr, "ERROR: Failed to allocate table rows\n");
        free(cs);
        return NULL;
    }

    // Allocate each row and initialize to zero
    for (int i = 0; i < d; i++) 
    {
        cs->table[i] = (int32_t *)calloc(w, sizeof(int32_t));
        if (!cs->table[i]) {
            fprintf(stderr, "ERROR: Failed to allocate table row %d\n", i);
            // Cleanup already allocated rows
            for (int j = 0; j < i; j++) free(cs->table[j]);
            free(cs->table);
            free(cs);
            return NULL;
        }
    }

    // Allocate and generate random seeds for each row's hash function
    cs->seeds = (uint32_t *)malloc(d * sizeof(uint32_t));
    if (!cs->seeds) {
        fprintf(stderr, "ERROR: Failed to allocate seeds\n");
        for (int i = 0; i < d; i++) free(cs->table[i]);
        free(cs->table);
        free(cs);
        return NULL;
    }
    
    for (int i = 0; i < d; i++) 
    {
        cs->seeds[i] = rand(); // Random seed for each row
    }

    return cs;
}

// FUNCTION: cs_create_with_seeds
/*
 Creates a Count Sketch with SPECIFIC seeds (for MPI consistency).
 In MPI, all ranks must use identical hash functions. Rank 0 generates
 seeds and broadcasts them, then all ranks call this function.
 */
CountSketch* cs_create_with_seeds(int d, int w, uint32_t *seeds) 
{
    CountSketch *cs = (CountSketch *)malloc(sizeof(CountSketch));
    if (!cs) {
        fprintf(stderr, "ERROR: Failed to allocate CountSketch\n");
        return NULL;
    }
    
    cs->d = d;
    cs->w = w;

    // Allocate 2D table
    cs->table = (int32_t **)malloc(d * sizeof(int32_t *));
    if (!cs->table) {
        free(cs);
        return NULL;
    }

    for (int i = 0; i < d; i++) 
    {
        cs->table[i] = (int32_t *)calloc(w, sizeof(int32_t));
        if (!cs->table[i]) {
            for (int j = 0; j < i; j++) free(cs->table[j]);
            free(cs->table);
            free(cs);
            return NULL;
        }
    }

    // Copy the provided seeds
    cs->seeds = (uint32_t *)malloc(d * sizeof(uint32_t));
    if (!cs->seeds) {
        for (int i = 0; i < d; i++) free(cs->table[i]);
        free(cs->table);
        free(cs);
        return NULL;
    }
    
    // Copy seeds, don't point to them!
    // Each rank may have seeds at different memory locations.
    memcpy(cs->seeds, seeds, d * sizeof(uint32_t));

    return cs;
}

// FUNCTION: cs_update
/*
 Updates the Count Sketch with a new item from the data stream.
 
 For each row i (0 to d-1):
     1. Compute bucket = hash(item, seed[i]) mod w
     2. Compute sign = +1 or -1 from a second hash
     3. table[i][bucket] += sign
 
 We have to use two hash functions for:
   - bucket: Determines WHICH counter to update
   - sign: Determines if we ADD or SUBTRACT 1
 The sign helps cancel out "hash collisions" in expectation.
 If two different items hash to the same bucket but have opposite signs,
 they will cancel out, reducing estimation error.
 */
void cs_update(CountSketch *cs, const char *item) 
{
    for (int i = 0; i < cs->d; i++) 
    {
        // Hash for bucket selection using row's seed
        uint32_t h_bucket = hash_fnv1a(item, cs->seeds[i]);
        
        // Hash for sign using a different seed (offset by constant)
        uint32_t h_sign = hash_fnv1a(item, cs->seeds[i] + 0xDEADBEEF);
        
        // Map hash to bucket index [0, w-1]
        int bucket = h_bucket % cs->w;
        
        // Map hash to sign: even → +1, odd → -1
        int sign = (h_sign % 2 == 0) ? 1 : -1;
        
        // Update the counter
        cs->table[i][bucket] += sign;
    }
}

// FUNCTION: cs_estimate
/* Estimates the frequency of an item.
    1. For each row, compute the same bucket and sign as in update
    2. Read the counter value and multiply by sign to "undo" the sign
    3. Return the MEDIAN of all d estimates
  
    We use the median because the mean would be affected by outliers (hash collisions)
    Median is robust: if majority of rows give correct estimate, we're good
    With d rows, we tolerate up to (d-1)/2 "bad" rows
 */
int32_t cs_estimate(CountSketch *cs, const char *item) 
{
    // Allocate array to store estimates from each row
    int32_t* estimates = (int32_t *)calloc(cs->d, sizeof(int32_t));
    if (!estimates) return 0;
    
    for (int i = 0; i < cs->d; i++) 
    {
        // Recompute the bucket and sign (same hashes as update)
        uint32_t h_bucket = hash_fnv1a(item, cs->seeds[i]);
        uint32_t h_sign = hash_fnv1a(item, cs->seeds[i] + 0xDEADBEEF);
        
        int bucket = h_bucket % cs->w;
        int sign = (h_sign % 2 == 0) ? 1 : -1;
        
        // Multiply by sign to recover the original count
        estimates[i] = cs->table[i][bucket] * sign;
    }

    // Sort estimates to find median
    qsort(estimates, cs->d, sizeof(int32_t), compare_ints);
    
    int32_t result;
    if (cs->d % 2 == 1) 
    {
        // Odd number of rows: take middle element
        result = estimates[cs->d / 2];
    } 
    else 
    {
        // Even number of rows: average of two middle elements
        result = (estimates[(cs->d / 2) - 1] + estimates[cs->d / 2]) / 2;
    }

    free(estimates);
    return result;
}

// FUNCTION: cs_destroy
/*
 Frees all memory allocated for a Count Sketch.
 */
void cs_destroy(CountSketch *cs) 
{
    if (!cs) return;
    
    // Free each row
    if (cs->table) {
        for (int i = 0; i < cs->d; i++) 
        {
            free(cs->table[i]);
        }
        free(cs->table);
    }
    
    // Free seeds array
    free(cs->seeds);
    
    // Free the struct itself
    free(cs);
}

// FUNCTION: cs_merge
/*
 Merges source Count Sketch into target (target += source).
 This is the key operation for MPI parallelization.
 Because Count Sketch is LINEAR (uses only addition), we can:
   1. Split data among P processors
   2. Each processor builds a partial Count Sketch
   3. Sum all partial sketches to get the final result
 
 MATHEMATICAL PROOF:
   Let CS(X) be the Count Sketch from processing items X.
   If X = X1 ∪ X2 (disjoint partition), then:
     CS(X) = CS(X1) + CS(X2)
   
 Where + means element-wise addition of tables.
 */
void cs_merge(CountSketch *target, const CountSketch *source) 
{
    // Verify dimensions match
    if (target->d != source->d || target->w != source->w) {
        fprintf(stderr, "ERROR: Cannot merge Count Sketches with different dimensions\n");
        fprintf(stderr, "  Target: %d x %d, Source: %d x %d\n", 
                target->d, target->w, source->d, source->w);
        return;
    }
    
    // Element-wise addition
    for (int i = 0; i < target->d; i++) 
    {
        for (int j = 0; j < target->w; j++) 
        {
            target->table[i][j] += source->table[i][j];
        }
    }
}

// FUNCTION: cs_flatten
/*
 Converts 2D table to 1D array for MPI communication.
 
 MPI_Reduce, MPI_Gather, etc. work with contiguous memory blocks.
 Our 2D table (int32_t**) is an array of pointers, and then is not contiguous.
 
 The memory layout looks like this (row-major order):
   table[0][0], table[0][1], ..., table[0][w-1],   <- row 0
   table[1][0], table[1][1], ..., table[1][w-1],   <- row 1
   ...
   table[d-1][0], ..., table[d-1][w-1]             <- row d-1
 
 Index mapping:
   flat[i * w + j] = table[i][j]
 */
int32_t* cs_flatten(const CountSketch *cs) 
{
    int total_size = cs->d * cs->w;
    
    // Allocate contiguous 1D array
    int32_t *flat = (int32_t *)malloc(total_size * sizeof(int32_t));
    if (!flat) {
        fprintf(stderr, "ERROR: Failed to allocate flat array\n");
        return NULL;
    }
    
    // Copy row by row into contiguous memory
    for (int i = 0; i < cs->d; i++) 
    {
        /*
         memcpy is faster than element-by-element copy
         Source: table[i] (w elements)
         Destination: flat + i*w (offset by i rows)
         */
        memcpy(&flat[i * cs->w], cs->table[i], cs->w * sizeof(int32_t));
    }
    
    return flat;
}

// FUNCTION: cs_unflatten
/*
 Creates a Count Sketch from a 1D array (inverse of cs_flatten).
 Used at rank 0 after MPI_Reduce to reconstruct the merged table.
 */
CountSketch* cs_unflatten(const int32_t *flat, int d, int w, uint32_t *seeds) 
{
    // Create a new Count Sketch with the given seeds
    CountSketch *cs = cs_create_with_seeds(d, w, seeds);
    if (!cs) return NULL;
    
    // Copy data from 1D array to 2D table
    for (int i = 0; i < d; i++) 
    {
        // Copy w elements starting at flat[i*w] into table[i]
        memcpy(cs->table[i], &flat[i * w], w * sizeof(int32_t));
    }
    
    return cs;
}

// FUNCTION: print_timings
/*
 Displays benchmark timing results in a formatted way.   
 */
void print_timings(const BenchmarkTimings *timings, int rank, int size, int n_items) 
{
    if (rank != 0) return;  /* Only rank 0 prints */
    
    printf("\n============= BENCHMARK RESULTS =============\n");
    printf("MPI Processes: %d\n", size);
    printf("Total Items:   %d\n", n_items);
    printf("\nPhase Timings:\n");
    printf("  Read:     %10.6f s\n", timings->time_read);
    printf("  Bcast:    %10.6f s\n", timings->time_bcast);
    printf("  Scatter:  %10.6f s\n", timings->time_scatter);
    printf("  Compute:  %10.6f s\n", timings->time_compute);
    printf("  Reduce:   %10.6f s\n", timings->time_reduce);
    printf("  Estimate: %10.6f s\n", timings->time_estimate);
    printf("\nTOTAL:      %10.6f s\n", timings->time_total);
    printf("=============================================\n\n");
}

// FUNCTION: write_timings_csv
/*
 Appends timing data to a CSV file for scalability analysis.
 Creates file with headers if it doesn't exist.
 
 This enables easy plotting of:
   - Strong scaling: Fixed N, varying P → plot speedup
   - Weak scaling: Fixed N/P, varying P → plot efficiency
 */
void write_timings_csv(const char *filename, const BenchmarkTimings *timings, 
                       int size, int n_items) 
{
    FILE *f;
    int file_exists = 0;
    
    // Check if file exists (to know whether to write headers)
    f = fopen(filename, "r");
    if (f) {
        file_exists = 1;
        fclose(f);
    }
    
    // Open in append mode
    f = fopen(filename, "a");
    if (!f) {
        fprintf(stderr, "ERROR: Cannot open file %s for writing\n", filename);
        return;
    }
    
    // Write header row if new file
    if (!file_exists) {
        fprintf(f, "processes,items,time_read,time_bcast,time_scatter,"
                   "time_compute,time_reduce,time_estimate,time_total\n");
    }
    
    // Write data row
    fprintf(f, "%d,%d,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f\n",
            size, n_items,
            timings->time_read,
            timings->time_bcast,
            timings->time_scatter,
            timings->time_compute,
            timings->time_reduce,
            timings->time_estimate,
            timings->time_total);
    
    fclose(f);
}
