#include "count_sketch.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <omp.h>
// Hybrid implementation of Count Sketch Algorithm using MPI + OpenMP
#define MAX_ITEM_LENGTH 256

// Internal FNV-1a Hash helper
static uint32_t hash_fnv1a(const char *key, uint32_t seed)
{
    uint32_t hash = 2166136261U ^ seed;
    while (*key) {
        hash ^= (uint8_t)*key++;
        hash *= 16777619U;
    }
    return hash;
}

static int compare_ints(const void *a, const void *b)
{
   int32_t arg1 = *(const int32_t *)a;
    int32_t arg2 = *(const int32_t *)b;
    if (arg1 < arg2) return -1;
    if (arg1 > arg2) return 1;
    return 0;
}

CountSketch* cs_create(int d, int w)
{
    CountSketch *cs = (CountSketch *)malloc(sizeof(CountSketch));
    cs->d = d;
    cs->w = w;

    //Allocating one contiguous block for the table
    int32_t *actual_data = calloc(d * w, sizeof(int32_t));

    cs->table = (int32_t **)malloc(d * sizeof(int32_t *));

    for (int i = 0; i < d; i++)
    {
        cs->table[i] = &actual_data[i * w];
    }

    cs->seeds = (uint32_t *)malloc(d * sizeof(uint32_t));
    return cs;
}

void cs_update(CountSketch *cs, const char *item)
{
    for (int i = 0; i < cs->d; i++)
    {
        // Use row seed for bucket, row seed + constant for sign
        uint32_t h_bucket = hash_fnv1a(item, cs->seeds[i]);
        uint32_t h_sign = hash_fnv1a(item, cs->seeds[i] + 0xDEADBEEF);

        int bucket = h_bucket % cs->w;
        int sign = (h_sign % 2 == 0) ? 1 : -1;
        #pragma omp atomic
        cs->table[i][bucket] += sign;
    }
}

int32_t cs_estimate(CountSketch *cs, const char *item)
{
    int32_t *estimates = (int32_t *)malloc(cs->d * sizeof(int32_t));

    for (int i = 0; i < cs->d; i++)
    {
        uint32_t h_bucket = hash_fnv1a(item, cs->seeds[i]);
        uint32_t h_sign = hash_fnv1a(item, cs->seeds[i] + 0xDEADBEEF);

        int bucket = h_bucket % cs->w;
        int sign = (h_sign % 2 == 0) ? 1 : -1;

        estimates[i] = cs->table[i][bucket] * sign;
    }

    // Sort estimates to find median
    qsort(estimates, cs->d, sizeof(int32_t), compare_ints);
    int32_t median;
    if (cs->d % 2 == 0) {
        median = (estimates[cs->d / 2 - 1] + estimates[cs->d / 2]) / 2;
    } else {
        median = estimates[cs->d / 2];
    }

    free(estimates);
    return median;
}

void cs_destroy(CountSketch *cs)
{
    free(cs->table[0]); // Free the one big block of data
    free(cs->table);    // Free the array of pointers
    free(cs->seeds);
    free(cs);
}
