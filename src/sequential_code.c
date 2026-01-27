#include <mpi.h>
#include <stdio.h>

int main(int argc, char** argv) {
    // Inizializza l'ambiente MPI
    MPI_Init(&argc, &argv);

    // Ottieni il numero totale di processi (quanti ne hai lanciato con -np)
    int world_size;
    MPI_Comm_size(MPI_COMM_WORLD, &world_size);

    // Ottieni il "rank" (l'ID) del processo corrente (0, 1, 2, ...)
    int world_rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);

    // Ottieni il nome del processore (utile se giri su un cluster, qui sarà il nome del tuo Mac)
    char processor_name[MPI_MAX_PROCESSOR_NAME];
    int name_len;
    MPI_Get_processor_name(processor_name, &name_len);

    // Stampa di prova
    printf("👋 Ciao! Sono il processo %d di %d in esecuzione su %s\n",
           world_rank, world_size, processor_name);

    // Chiudi l'ambiente MPI
    MPI_Finalize();
    return 0;
}