#!/bin/bash
# Strong Scaling Benchmark Script - MPI Implementation
#
# Usage:
#   ./scripts/benchmark_performance_mpi.sh
#
# Output:
#   results/performance_mpi_<lines>.csv

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

# Determine total lines for CSV filename
TOTAL_LINES=$(wc -l < "$INPUT_FILE" | xargs)
CSV_FILE="${RESULTS_DIR}/performance_mpi_${TOTAL_LINES}.csv"

# --- TERMINAL TABLE HEADER ---
echo "| Processes | N_Threads | Serial Time | Compute Time | Compute + Communication Time | Speedup Compute | Efficiency Compute | Speedup Communication | Efficiency Communication |"
echo "|-----------|-----------|-------------|--------------|------------------------------|-----------------|--------------------|-----------------------|--------------------------|"

# --- CSV FILE INITIALIZATION ---
echo "Processes,N_Threads,Serial_Time,Compute_Time,Total_Time,Speedup_Compute,Efficiency_Compute,Speedup_Comm,Efficiency_Comm" > "$CSV_FILE"

# --- SCALABILITY LOOP ---
# For MPI, we only vary processes (no threads)
for np in 2 4 8 16 32 64
do
    # scaling_mode = 0 (Strong Scaling)
    mpirun -np $np $EXE $TRIALS $BUCKETS $INPUT_FILE 0 | \
        grep "|" | \
        grep -v "Processes" | \
        grep -v -- "---" | \
        grep -v "Total lines" | \
        grep -v "Estimated frequency"
done

echo "---------------------------------------------------------------------------------------------------------------------------------------------------------------------------"
echo "Strong scaling study completed successfully."
echo "CSV results saved to: $CSV_FILE"
