#include <stdio.h>
#include <stdlib.h>
#include "include/count_sketch.h"
#include <string.h>
#include <time.h>
#include <mpi.h>

#define MAX_ITEM_LENGTH 256

int main(int argc, char** argv) 
{
    MPI_Init(&argc, &argv);
    //Serial implementation of the count sketch algorithm would go here.
    srand(time(NULL));

    if(argc < 4) 
    {
        printf("Usage: ./app_seq %s <num_trials> <num_buckets> <input_file_path>\n", argv[0]);
        MPI_Finalize();
        return 1;
    }

    int depth = atoi(argv[1]);
    int width = atoi(argv[2]);
    char* input_file = argv[3];

    // Rows are the independent trials, while columns represent the hash buckets 
    CountSketch *cs = cs_create(depth, width); 


    double wt1, wt2;
    FILE *file = fopen(input_file, "r");
    if (file == NULL)
    {
        perror("Unable to open file!");
        return EXIT_FAILURE;
    }

    char item[MAX_ITEM_LENGTH];
    int total_lines = 0;
    //Count lines in the file
    while (fgets(item, sizeof(item), file))
    {
        total_lines++;
    }
printf("Total lines in input file: %d\n", total_lines);
    char* items = malloc(total_lines * MAX_ITEM_LENGTH);
    int i = 0;
    while (fgets(item, sizeof(item), file))
    {
        // Remove newline character if present
        item[strcspn(item, "\n")] = 0;
        strcpy(&items[i * MAX_ITEM_LENGTH], item);
        i++;
    }
    fclose(file);   

    wt1 = MPI_Wtime();
    for(int j=0; j<i; j++) 
    {
        cs_update(cs, &items[j * MAX_ITEM_LENGTH]);
    }   
    wt2 = MPI_Wtime();

    // Estimate frequencies of the filed items
    
    for (int j = 0; j < i; j++) 
    {
        int32_t estimate = cs_estimate(cs, &items[j * MAX_ITEM_LENGTH]);
        printf("Estimated frequency of %s: %d\n", &items[j * MAX_ITEM_LENGTH], estimate);
        //Print in the file estimates.txt
        FILE *est_file = fopen("results/estimates.txt", "a");
        if (est_file == NULL)
        {
            perror("Unable to open estimates file!");
            return EXIT_FAILURE;
        }
        fclose(est_file);
    }
    printf("Time taken for updates: %f seconds\n", (wt2 - wt1));
    cs_destroy(cs); // Free allocated memory
    free(items);
    MPI_Finalize();
    return 0;
}
