#!/bin/bash
# Weak Scaling Benchmark Script - MPI Implementation
#
# Weak Scaling Formula: Sw = T1(N) / Tp(N*P)
# - T1: execution time on 1 processor for problem size N
# - Tp: execution time on P processors for problem size N*P
# - Goal: Sw ≈ 1 (constant time as problem and resources scale together)
#
# Usage:
#   ./scripts/weak_scaling_mpi.sh
#
# Output:
#   results/weak_scaling_mpi_8388608.csv

# Navigate to project root
cd "$(dirname "$0")/.." || exit

# --- CONFIGURATION ---
EXE="./build/app_mpi"
TRIALS=5
BUCKETS=1000
INPUT_FILE="datasets/input_data_8388608.txt"
RESULTS_DIR="results"

# Ensure directories exist
mkdir -p $RESULTS_DIR

# Check if input file exists
if [ ! -f "$INPUT_FILE" ]; then
    echo "Error: Input file $INPUT_FILE not found."
    echo "Generate it with: python src/generator/generator.py --items 8388608 --output $INPUT_FILE"
    exit 1
fi

# Check if executable exists
if [ ! -f "$EXE" ]; then
    echo "Error: Executable $EXE not found."
    echo "Build it with: cd build && cmake .. && make"
    exit 1
fi

TOTAL_LINES=$(wc -l < "$INPUT_FILE" | xargs)
CSV_FILE="${RESULTS_DIR}/weak_scaling_mpi_${TOTAL_LINES}.csv"

# --- CSV FILE INITIALIZATION ---
echo "Processes,N_Threads,Total_Lines,Serial_Time,Compute_Time,Compute_Communication_Time,Weak_Scaling_Compute,Weak_Scaling_Communication" > "$CSV_FILE"

# --- TEST WEAK SCALING ---
echo -e "\nStarting Weak Scaling Tests..."
echo "| Processes | N_Threads | Total Lines | Serial Time | Compute Time | Compute + Communication Time | Weak Scaling Compute | Weak Scaling Communication |"
echo "|-----------|-----------|-------------|-------------|--------------|------------------------------|----------------------|----------------------------|"

# Weak scaling: each process gets fixed work (8M/64 items per process)
for np in 2 4 8 16 32 64
do
    # scaling_mode = 1 (Weak Scaling)
    mpirun -np $np $EXE $TRIALS $BUCKETS $INPUT_FILE 1 | \
        grep "|" | grep -v "Processes" | grep -v -- "---" | \
        grep -v "Total lines" | grep -v "Estimated frequency"
done

echo "CSV results saved to: $CSV_FILE"
echo "Weak scaling study completed successfully."
