#!/bin/bash
#   
# Benchmark runner for Strong and Weak Scalability Tests
# 
# This script runs benchmarks for:
#   1. STRONG SCALING: Fixed problem size, varying processes
#   2. WEAK SCALING: Fixed work per process, varying processes
#
#   ./scripts/run_benchmarks.sh [strong|weak|all]
#
# Output:
#   results/benchmark_results.csv - Timing data for analysis
#
# For cluster (PBS):
#   Use this for local testing. For cluster, create a PBS script.
#

set -e

# Configuration
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
PROJECT_ROOT="$( cd "$SCRIPT_DIR/.." && pwd )"
BIN_DIR="$PROJECT_ROOT/bin"
DATA_DIR="$PROJECT_ROOT/datasets"
RESULTS_DIR="$PROJECT_ROOT/results"
GENERATOR="$PROJECT_ROOT/src/generator/generator.py"

# Benchmark parameters
PROCESS_COUNTS=(1 2 4 8)  # Add 16, 32 on cluster with more cores
STRONG_PROBLEM_SIZE=100000   # Fixed size for strong scaling (adjust based on your cluster)
WEAK_PER_PROCESS=25000       # Items per process for weak scaling

# Number of runs per configuration (for averaging)
NUM_RUNS=3

# Colors
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

echo -e "${GREEN}========================================${NC}"
echo -e "${GREEN}Count Sketch MPI - Benchmark Runner${NC}"
echo -e "${GREEN}========================================${NC}"

# Ensure executables exist
if [ ! -f "$BIN_DIR/app_mpi" ] && [ ! -f "$BIN_DIR/app_mpi.exe" ]; then
    echo "ERROR: app_mpi not found in $BIN_DIR"
    echo "Please run ./scripts/build.sh first"
    exit 1
fi

# Create results directory
mkdir -p "$RESULTS_DIR"

# Detect executable name
if [ -f "$BIN_DIR/app_mpi" ]; then
    MPI_EXE="$BIN_DIR/app_mpi"
else
    MPI_EXE="$BIN_DIR/app_mpi.exe"
fi

# Function: run_strong_scaling
# Strong scaling: Fixed problem size, measure speedup as P increases
# Ideal: Tₚ = T₁/P → Speedup = P
run_strong_scaling() {
    echo -e "\n${YELLOW}=== STRONG SCALING ===${NC}"
    echo "Problem size: $STRONG_PROBLEM_SIZE items"
    echo "Process counts: ${PROCESS_COUNTS[*]}"
    
    # Generate dataset if needed
    DATASET="$DATA_DIR/strong_${STRONG_PROBLEM_SIZE}.txt"
    if [ ! -f "$DATASET" ]; then
        echo "Generating dataset: $DATASET"
        python3 "$GENERATOR" --items $STRONG_PROBLEM_SIZE --output "$DATASET"
    fi
    
    # Run benchmarks
    for P in "${PROCESS_COUNTS[@]}"; do
        echo -e "\n${GREEN}Running with $P processes...${NC}"
        for run in $(seq 1 $NUM_RUNS); do
            echo "  Run $run/$NUM_RUNS"
            mpirun -np $P "$MPI_EXE" "$DATASET" --benchmark
        done
    done
    
    echo -e "\n${GREEN}Strong scaling completed!${NC}"
}

# Function: run_weak_scaling
# Weak scaling: Fixed work per process, measure efficiency as P increases
# Ideal: Tₚ ≈ T₁ → Efficiency = T₁/Tₚ ≈ 1
run_weak_scaling() {
    echo -e "\n${YELLOW}=== WEAK SCALING ===${NC}"
    echo "Items per process: $WEAK_PER_PROCESS"
    echo "Process counts: ${PROCESS_COUNTS[*]}"
    
    # Generate datasets of increasing size
    for P in "${PROCESS_COUNTS[@]}"; do
        SIZE=$((WEAK_PER_PROCESS * P))
        DATASET="$DATA_DIR/weak_${SIZE}.txt"
        
        if [ ! -f "$DATASET" ]; then
            echo "Generating dataset: $DATASET ($SIZE items)"
            python3 "$GENERATOR" --items $SIZE --output "$DATASET"
        fi
    done
    
    # Run benchmarks
    for P in "${PROCESS_COUNTS[@]}"; do
        SIZE=$((WEAK_PER_PROCESS * P))
        DATASET="$DATA_DIR/weak_${SIZE}.txt"
        
        echo -e "\n${GREEN}Running with $P processes ($SIZE items)...${NC}"
        for run in $(seq 1 $NUM_RUNS); do
            echo "  Run $run/$NUM_RUNS"
            mpirun -np $P "$MPI_EXE" "$DATASET" --benchmark
        done
    done
    
    echo -e "\n${GREEN}Weak scaling completed!${NC}"
}

# Main
case "${1:-all}" in
    strong)
        run_strong_scaling
        ;;
    weak)
        run_weak_scaling
        ;;
    all)
        run_strong_scaling
        run_weak_scaling
        ;;
    *)
        echo "Usage: $0 [strong|weak|all]"
        exit 1
        ;;
esac

echo -e "\n${GREEN}========================================${NC}"
echo "Benchmark results saved to: $RESULTS_DIR/benchmark_results.csv"
echo ""
echo "Next steps:"
echo "1. Copy results from cluster if running remotely"
echo "2. Use src/utils/data analysis/plotter.py to generate graphs"
echo -e "${GREEN}========================================${NC}"
