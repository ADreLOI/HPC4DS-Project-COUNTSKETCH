#!/bin/bash
# Weak Scaling Multi-Size Benchmark - MPI Implementation
#
# Runs weak scaling benchmarks on 3 dataset sizes:
#   - 500K (524288)
#   - 1M (1048576)  
#   - 8M (8388608)
#
# Output:
#   results/weak_scaling_mpi_524288.csv
#   results/weak_scaling_mpi_1048576.csv
#   results/weak_scaling_mpi_8388608.csv

# Navigate to project root
cd "$(dirname "$0")/.." || exit

EXE="./build/app_mpi"
TRIALS=5
BUCKETS=1000
RESULTS_DIR="results"

mkdir -p $RESULTS_DIR

# Check if executable exists
if [ ! -f "$EXE" ]; then
    echo "Error: Executable $EXE not found."
    exit 1
fi

# Dataset sizes to test
declare -a SIZES=("524288" "1048576" "8388608")
declare -a LABELS=("500K" "1M" "8M")

for i in "${!SIZES[@]}"; do
    SIZE=${SIZES[$i]}
    LABEL=${LABELS[$i]}
    INPUT_FILE="datasets/input_data_${SIZE}.txt"
    CSV_FILE="${RESULTS_DIR}/weak_scaling_mpi_${SIZE}.csv"
    
    # Check if input file exists
    if [ ! -f "$INPUT_FILE" ]; then
        echo "Warning: $INPUT_FILE not found, skipping $LABEL test."
        continue
    fi
    
    echo ""
    echo "========================================="
    echo "Weak Scaling: $LABEL dataset ($SIZE items)"
    echo "========================================="
    
    # Initialize CSV
    echo "Processes,N_Threads,Total_Lines,Serial_Time,Compute_Time,Compute_Communication_Time,Weak_Scaling_Compute,Weak_Scaling_Communication" > "$CSV_FILE"
    
    # Header
    echo "| Processes | N_Threads | Total Lines | Serial Time | Compute Time | Compute + Communication Time | Weak Scaling Compute | Weak Scaling Communication |"
    echo "|-----------|-----------|-------------|-------------|--------------|------------------------------|----------------------|----------------------------|"
    
    # Run for each process count (weak scaling mode = 1)
    for np in 2 4 8 16 32 64; do
        mpirun -np $np $EXE $TRIALS $BUCKETS $INPUT_FILE 1 2>/dev/null | \
            grep "|" | \
            grep -v "Processes" | \
            grep -v -- "---" | \
            grep -v "Total lines" | \
            grep -v "Estimated frequency"
    done
    
    echo "Results saved to: $CSV_FILE"
done

echo ""
echo "========================================="
echo "All weak scaling tests completed!"
echo "========================================="
