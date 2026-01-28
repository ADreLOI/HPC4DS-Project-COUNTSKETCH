#ifndef COUNT_SKETCH_H
#define COUNT_SKETCH_H

#include <stdint.h>

typedef struct 
{
    int d;               // Number of rows (Depth)
    int w;               // Number of columns (Width)
    int32_t **table;     // 2D array of counters
    uint32_t *seeds;     // Unique seeds for each row's hash functions
} CountSketch;

// Memory Management
CountSketch* cs_create(int d, int w);
void cs_destroy(CountSketch *cs);

// Core Operations
void cs_update(CountSketch *cs, const char *item);
int32_t cs_estimate(CountSketch *cs, const char *item);

// Helper for MPI: Merging two sketches
void cs_merge(CountSketch *target, const CountSketch *source);

#endif