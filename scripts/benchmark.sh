#!/bin/bash
#PBS -N CountSketch_Scaling_Study
#PBS -l nodes=2:ppn=32
#PBS -l walltime=02:00:00
#PBS -q workq
#PBS -j oe

# Load necessary modules (Uncomment and modify based on your cluster environment)
# module load openmpi
# module load gcc

# Navigate to the directory where the job was submitted
#cd $PBS_O_WORKDIR

# --- CONFIGURATION ---
EXE="./build/app_hybrid"
TRIALS=5
BUCKETS=1000
INPUT_FILE="datasets/input_data_8388608.txt"
RESULTS_DIR="results"

# Determine the total number of lines for the CSV filename
# xargs is used here to trim any leading/trailing whitespace from wc output
TOTAL_LINES=$(wc -l < "$INPUT_FILE" | xargs)
CSV_FILE="${RESULTS_DIR}/performance_hybrid_${TOTAL_LINES}.csv"

# Ensure the results directory exists
mkdir -p $RESULTS_DIR

# --- TERMINAL TABLE HEADER ---
# This prints the header once at the start of the console output
echo "| Processes | N_Threads | Serial Time | Compute Time | Compute + Communication Time | Speedup Compute | Efficiency Compute | Speedup Communication | Efficiency Communication |"
echo "|-----------|-----------|-------------|--------------|------------------------------|-----------------|--------------------|-----------------------|--------------------------|"

# --- CSV FILE INITIALIZATION ---
# Create/Overwrite the CSV file with the comma-separated header
echo "Processes,N_Threads,Serial_Time,Compute_Time,Total_Time,Speedup_Compute,Efficiency_Compute,Speedup_Comm,Efficiency_Comm" > "$CSV_FILE"

# --- SCALABILITY LOOP ---
# Outer loop: Iterate through MPI processes (Number of Ranks)
for np in 2 4 8 16 32 64
do
    # Inner loop: Iterate through OpenMP threads per process
    for nt in 2 4 8 16 32 64
    do
        # Set the environment variable for OpenMP thread count
        export OMP_NUM_THREADS=$nt
        
        # Execute the Hybrid application
        # 1. We run the command via mpirun.
        # 2. We use grep to filter the output:
        #    - Keep lines containing '|' (your table rows)
        #    - Ignore lines containing 'Processes' (to avoid repeating the header)
        #    - Ignore lines containing '---' (to avoid repeating the separator)
        mpirun -np $np $EXE $TRIALS $BUCKETS $INPUT_FILE $nt | grep "|" | grep -v "Processes" | grep -v "---"
        
    done
done

echo "---------------------------------------------------------------------------------------------------------------------------------------------------------------------------"
echo "Scaling study completed successfully."
echo "Results have been saved to: $CSV_FILE"