// COUNT SKETCH - MPI UTILITIES HEADER
/* 
  This header provides MPI-specific functions for parallelizing Count Sketch.
  
  STRATEGY: Local Table Per Processor
  1. Each MPI rank maintains its OWN local Count Sketch table
  2. Each rank processes a CHUNK of the input data (via MPI_Scatterv)
  3. All ranks use the SAME hash seeds (via MPI_Bcast) for consistency
  4. Partial tables are SUMMED at rank 0 (via MPI_Reduce with MPI_SUM)
  
  This works efficently because:
  Count Sketch uses additive updates: table[i][j] += sign
  Therefore: SUM(partial_tables) == table_from_all_items

  e.g.:
  Rank 0 processes "apple" → table[2][5] += 1
  Rank 1 processes "apple" → table[2][5] += 1  
  After MPI_Reduce: table[2][5] = 2 (correct count!)
*/

#ifndef COUNT_SKETCH_MPI_H
#define COUNT_SKETCH_MPI_H

#include <stdint.h>
#include <mpi.h>
#include "count_sketch.h"

// STRUCT: BenchmarkTimings
/* 
  Stores timing information for different phases of the MPI algorithm.
  Used for performance analysis and scalability studies.
  
  All times are in SECONDS (from MPI_Wtime()).
*/
typedef struct {
    double time_read;       /* Time to read input data (rank 0 only) */
    double time_bcast;      /* Time to broadcast seeds to all ranks */
    double time_scatter;    /* Time to scatter data chunks to all ranks */
    double time_compute;    /* Time for local Count Sketch updates */
    double time_reduce;     /* Time to reduce/merge partial tables */
    double time_estimate;   /* Time to estimate frequencies (rank 0 only) */
    double time_total;      /* Total wall-clock time */
} BenchmarkTimings;

// cs_create_with_seeds
/* 
  Creates a Count Sketch with SPECIFIC seeds instead of random ones.
  In MPI, all ranks MUST use the same hash functions. This means they need
  identical seeds. Rank 0 generates seeds and broadcasts them, then all
  ranks create their local Count Sketch with these shared seeds.
  
  It uses as parameters:
    d     - Number of rows (depth) - controls accuracy
    w     - Number of columns (width) - controls memory usage
    seeds - Array of d seeds (must be identical across all ranks!)
  
  And it returns:
    Pointer to newly allocated CountSketch with the given seeds
  
  e.g. :
    uint32_t seeds[5] = {123, 456, 789, 101, 112};
    CountSketch *cs = cs_create_with_seeds(5, 1000, seeds);
*/
CountSketch* cs_create_with_seeds(int d, int w, uint32_t *seeds);

// cs_merge
/* 
  Merges (adds) the source Count Sketch into the target.
  
  target->table[i][j] += source->table[i][j]  for all i, j
  
  After MPI_Reduce, we need to combine partial tables. Since Count Sketch
  is linear (additive), we simply sum corresponding cells.
  This operation must respect some fundamentals preconditions:
    - target and source must have the SAME dimensions (d, w)
    - target and source must use the SAME seeds
  
  It uses as parameters:
    target - The Count Sketch to merge INTO (will be modified)
    source - The Count Sketch to merge FROM (unchanged)
  
  e.g.:
    // Rank 0 after receiving partial tables:
    cs_merge(final_table, partial_from_rank_1);
    cs_merge(final_table, partial_from_rank_2);
*/
void cs_merge(CountSketch *target, const CountSketch *source);

// cs_flatten
/* 
  Converts the 2D Count Sketch table to a 1D array for MPI communication.
  
  MPI works most efficiently with contiguous memory blocks. Our Count Sketch
  table is a 2D array (array of pointers), which is NOT contiguous.
  
  We convert: table[d][w] → flat[d * w]
  Using ROW-MAJOR order: flat[i * w + j] = table[i][j]
  
  It uses as parameters:
    cs - The Count Sketch to flatten
  
  And it returns:
    Newly allocated int32_t array of size (d * w)
    the most important aspect is that the CALLER must free this memory
  
  e.g.:
    int32_t *flat = cs_flatten(my_cs);
    MPI_Reduce(flat, result, d*w, MPI_INT32_T, MPI_SUM, 0, MPI_COMM_WORLD);
    free(flat);
*/
int32_t* cs_flatten(const CountSketch *cs);

// cs_unflatten
/* 
  Creates a Count Sketch from a 1D array (inverse of cs_flatten).
  
  After MPI_Reduce at rank 0, we have a flat array. We need to convert it
  back to a proper CountSketch structure to use cs_estimate().
  
  It uses as parameters:
    flat  - The 1D array of size (d * w) from cs_flatten or MPI_Reduce
    d     - Number of rows
    w     - Number of columns
    seeds - The hash seeds (same as used to create original sketches)
  
  It returns:
    Newly allocated CountSketch with the reconstructed table
  
  e.g.:
    // At rank 0 after MPI_Reduce:
    CountSketch *final = cs_unflatten(result_flat, d, w, seeds);
    int freq = cs_estimate(final, "apple");
 */
CountSketch* cs_unflatten(const int32_t *flat, int d, int w, uint32_t *seeds);

// print_timings
/* 
  Prints benchmark timing information in a formatted way.
  Must only be called from rank 0.
  
  It uses as parameters:
    timings - Pointer to BenchmarkTimings struct
    rank    - MPI rank (for labeling)
    size    - Total number of MPI processes
    n_items - Total number of items processed
 */
void print_timings(const BenchmarkTimings *timings, int rank, int size, int n_items);

// write_timings_csv
/* 
  Appends timing data to a CSV file for later analysis.
  Creates the file with headers if it doesn't exist.
  
  Disposition of the CSV:
    processes, items, time_read, time_bcast, time_scatter, time_compute, time_reduce, time_estimate, time_total
  
  It uses as parameters:
    filename - Path to CSV file
    timings  - Timing data
    size     - Number of MPI processes
    n_items  - Number of items processed
 */
void write_timings_csv(const char *filename, const BenchmarkTimings *timings, 
                       int size, int n_items);

#endif
