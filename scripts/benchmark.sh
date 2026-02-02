#!/bin/bash
#PBS -N CountSketch_Scaling_Study
#PBS -l nodes=2:ppn=32
#PBS -l walltime=02:00:00
#PBS -q workq
#PBS -j oe

# Navigate to the root of your project (one level up from /scripts)
cd "$(dirname "$0")/.." || exit

# --- CONFIGURATION ---
EXE="./build/app_hybrid"
TRIALS=5
BUCKETS=1000
INPUT_FILE="datasets/input_data_8388608.txt"
RESULTS_DIR="results"

# Ensure directories exist
mkdir -p $RESULTS_DIR

# Check if input file exists before starting
if [ ! -f "$INPUT_FILE" ]; then
    echo "Error: Input file $INPUT_FILE not found."
    exit 1
fi

# Determine the total number of lines for the CSV filename
TOTAL_LINES=$(wc -l < "$INPUT_FILE" | xargs)
CSV_FILE="${RESULTS_DIR}/performance_hybrid_${TOTAL_LINES}.csv"

# --- TERMINAL TABLE HEADER ---
echo "| Processes | N_Threads | Serial Time | Compute Time | Compute + Communication Time | Speedup Compute | Efficiency Compute | Speedup Communication | Efficiency Communication |"
echo "|-----------|-----------|-------------|--------------|------------------------------|-----------------|--------------------|-----------------------|--------------------------|"

# --- CSV FILE INITIALIZATION ---
# Using header names that match your C fprintf logic
echo "Processes,N_Threads,Serial_Time,Compute_Time,Total_Time,Speedup_Compute,Efficiency_Compute,Speedup_Comm,Efficiency_Comm" > "$CSV_FILE"

# --- SCALABILITY LOOP ---
for np in 2 4 8 16 32 64
do
    for nt in 2 4 8 16 32 64
    do
        export OMP_NUM_THREADS=$nt
        
        # FIX: Added -- to grep to prevent it from interpreting --- as an option
        # We also filter out "Total lines" and "Starting Serial" to keep the table clean
        mpirun -np $np $EXE $TRIALS $BUCKETS $INPUT_FILE $nt | \
            grep "|" | \
            grep -v "Processes" | \
            grep -v -- "---" | \
            grep -v "Total lines" | \
            grep -v "Starting Serial"
        
    done
done

echo "---------------------------------------------------------------------------------------------------------------------------------------------------------------------------"
echo "Scaling study completed successfully."
echo "CSV results saved to: $CSV_FILE"
