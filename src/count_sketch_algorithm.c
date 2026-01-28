#include "include/count_sketch.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

CountSketch* cs_create(int d, int w) 
{
    CountSketch *cs = (CountSketch *)malloc(sizeof(CountSketch));
    cs->d = d;
    cs->w = w;

    cs->table = (int32_t **)malloc(d * sizeof(int32_t *));

    for (int i = 0; i < d; i++) 
    {
        cs->table[i] = (int32_t *)calloc(w, sizeof(int32_t));
    }

    cs->seeds = (uint32_t *)malloc(d * sizeof(uint32_t));
    for (int i = 0; i < d; i++) 
    {
        cs->seeds[i] = rand(); // Random seed for each row
    }

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
        
        cs->table[i][bucket] += sign;
    }
}

int32_t cs_estimate(CountSketch *cs, const char *item) 
{
    int32_t estimates[cs->d];
    
    for (int i = 0; i < cs->d; i++) 
    {
        int32_t h_bucket = hash_fnv1a(item, cs->seeds[i]);
        int32_t h_sign = hash_fnv1a(item, cs->seeds[i] + 0xDEADBEEF);
        
        int bucket = h_bucket % cs->w;
        int sign = (h_sign % 2 == 0) ? 1 : -1;
        
        estimates[i] = cs->table[i][bucket] * sign;
    }

    // Return median estimate
    qsort(estimates, cs->d, sizeof(int32_t), (int (*)(const void *, const void *))strcmp);
    if (cs->d % 2 == 1) 
    {
        return estimates[cs->d / 2];
    } 
    else 
    {
        return (estimates[(cs->d / 2) - 1] + estimates[cs->d / 2]) / 2;
    }
}

void cs_destroy(CountSketch *cs) 
{
    for (int i = 0; i < cs->d; i++) 
    {
        free(cs->table[i]);
    }
    free(cs->table);
    free(cs->seeds);
    free(cs);
}
