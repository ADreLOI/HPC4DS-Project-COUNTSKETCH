#include <stdio.h>
#include <stdlib.h>
#include "include/count_sketch.h"
#include <string.h>
#include <time.h>

int main(int argc, char** argv) 
{
    //Serial implementation of the count sketch algorithm would go here.
    srand(time(NULL));
    CountSketch *cs = cs_create(9, 64); // Example: 5 rows, 100 columns

    // Short loop to demonstrate update and estimate
    /*
    const char *items[] = {"apple", "banana", "orange", "apple", "banana", "apple"};
    for (int i = 0; i < 6; i++) 
    {
        cs_update(cs, items[i]);
    } 
    */  
   // Long loop to simulate data stream using file input_data_10000.txt
    FILE *file = fopen("datasets/input_data_100.txt", "r");
    if (file == NULL)
    {
        perror("Unable to open file!");
        return EXIT_FAILURE;
    }
    char item[256];
    char items[10000][256];
    int i = 0;
    while (fgets(item, sizeof(item), file))
    {
        // Remove newline character if present
        item[strcspn(item, "\n")] = 0;
        cs_update(cs, item);
        strcpy(items[i], item);
        i++;
    }
    fclose(file);   

    // Estimate frequencies of the filed items
    
    for (int j = 0; j < i; j++) 
    {
        int32_t estimate = cs_estimate(cs, items[j]);
        printf("Estimated frequency of %s: %d\n", items[j], estimate);
        //Print in the file estimates.txt
        FILE *est_file = fopen("results/estimates.txt", "a");
        if (est_file == NULL)
        {
            perror("Unable to open estimates file!");
            return EXIT_FAILURE;
        }
        fprintf(est_file, "Estimated frequency of %s: %d\n", items[j], estimate);
        fclose(est_file);
    }

    cs_destroy(cs); // Free allocated memory

    return 0;
}