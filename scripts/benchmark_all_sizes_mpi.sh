#!/bin/bash
# Multi-Size Strong Scaling Benchmark - For comparison graph
#
# Runs strong scaling benchmarks on 3 dataset sizes:
#   - 500K (524288)
#   - 1M (1048576)
#   - 8M (8388608)
#
# Usage:
#   ./scripts/benchmark_all_sizes_mpi.sh
#
# Output:
#   results/performance_mpi_524288.csv
#   results/performance_mpi_1048576.csv
#   results/performance_mpi_8388608.csv

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
    echo "Build it with: cd build && cmake .. && make"
    exit 1
fi

# Dataset sizes to test
declare -a SIZES=("524288" "1048576" "8388608")
declare -a LABELS=("500K" "1M" "8M")

for i in "${!SIZES[@]}"; do
    SIZE=${SIZES[$i]}
    LABEL=${LABELS[$i]}
    INPUT_FILE="datasets/input_data_${SIZE}.txt"
    CSV_FILE="${RESULTS_DIR}/performance_mpi_${SIZE}.csv"
    
    # Check if input file exists
    if [ ! -f "$INPUT_FILE" ]; then
        echo "Warning: $INPUT_FILE not found, skipping $LABEL test."
        echo "Generate with: python src/generator/generator.py --items $SIZE --output $INPUT_FILE"
        continue
    fi
    
    echo ""
    echo "========================================="
    echo "Testing $LABEL dataset ($SIZE items)"
    echo "========================================="
    
    # Initialize CSV
    echo "Processes,N_Threads,Serial_Time,Compute_Time,Total_Time,Speedup_Compute,Efficiency_Compute,Speedup_Comm,Efficiency_Comm" > "$CSV_FILE"
    
    # Header
    echo "| Processes | N_Threads | Serial Time | Compute Time | Compute + Communication Time | Speedup Compute | Efficiency Compute | Speedup Communication | Efficiency Communication |"
    echo "|-----------|-----------|-------------|--------------|------------------------------|-----------------|--------------------|-----------------------|--------------------------|"
    
    # Run for each process count
    for np in 2 4 8 16 32 64; do
        mpirun -np $np $EXE $TRIALS $BUCKETS $INPUT_FILE 0 2>/dev/null | \
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
echo "All benchmark tests completed!"
echo "========================================="
echo ""
echo "Output files:"
for SIZE in "${SIZES[@]}"; do
    echo "  - results/performance_mpi_${SIZE}.csv"
done
