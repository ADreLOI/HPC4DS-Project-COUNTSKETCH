#include <mpi.h>
#include <stdio.h>

int main(int argc, char** argv) 
{
    MPI_Init(&argc, &argv);

    int world_size;
    MPI_Comm_size(MPI_COMM_WORLD, &world_size);

    int world_rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);

    char processor_name[MPI_MAX_PROCESSOR_NAME];
    int name_len;
    MPI_Get_processor_name(processor_name, &name_len);

    printf("Ciao! Sono il processo OMP %d di %d in esecuzione su %s\n",
           world_rank, world_size, processor_name);

    MPI_Finalize(); 
    return 0;
}