#!/bin/bash
# Benchmark Script - Matching Teammate's Hybrid Implementation Setup
#
# Parameters from teammate:
#   - Depth: 5
#   - Width: 1000  
#   - Items: 8,388,608 (8M = 2^23)
#   - MAX_ITEM_LEN: 64
#
# Usage:
#   chmod +x scripts/run_mpi_benchmark.sh
#   ./scripts/run_mpi_benchmark.sh

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

# Configuration - MATCH teammate's hybrid implementation
DEPTH=5
WIDTH=1000
ITEMS=8388608  # 2^23 = 8M items
INPUT_FILE="datasets/input_data_8M.txt"
RESULTS_CSV="results/mpi_benchmark_results.csv"

# Process counts to test (powers of 2)
PROCS=(1 4 8 16 32)

echo -e "${YELLOW}========================================${NC}"
echo -e "${YELLOW}MPI Count Sketch Benchmark${NC}"
echo -e "${YELLOW}========================================${NC}"
echo "Config: depth=$DEPTH, width=$WIDTH, items=$ITEMS"
echo ""

# Check if executable exists
if [ ! -f "./build/app_mpi" ]; then
    echo -e "${RED}ERROR: ./build/app_mpi not found${NC}"
    echo "Please build first: cd build && cmake .. && make"
    exit 1
fi

# Generate 8M dataset if it doesn't exist
if [ ! -f "$INPUT_FILE" ]; then
    echo -e "${YELLOW}Generating 8M item dataset...${NC}"
    python3 src/generator/generator.py --items $ITEMS --output $INPUT_FILE --seed 42
    echo -e "${GREEN}Dataset created: $INPUT_FILE${NC}"
fi

# Create results directory
mkdir -p results

# Write CSV header
echo "processes,items,depth,width,time_compute,time_total,speedup,efficiency" > $RESULTS_CSV

# Store baseline time for speedup calculation
BASELINE_TIME=0

echo -e "\n${YELLOW}Running benchmark tests...${NC}\n"
echo "| Processes | Compute Time | Total Time | Speedup | Efficiency |"
echo "|-----------|--------------|------------|---------|------------|"

for P in "${PROCS[@]}"; do
    # Run MPI with specified parameters
    OUTPUT=$(mpirun -np $P ./build/app_mpi $INPUT_FILE --depth $DEPTH --width $WIDTH --benchmark 2>&1)
    
    # Extract compute time (this is what teammate measures)
    COMPUTE_TIME=$(echo "$OUTPUT" | grep "Compute:" | head -1 | awk '{print $2}')
    TOTAL_TIME=$(echo "$OUTPUT" | grep "TOTAL:" | awk '{print $2}')
    
    # Handle different output formats
    if [ -z "$COMPUTE_TIME" ]; then
        # Try alternative parsing
        COMPUTE_TIME=$(echo "$OUTPUT" | grep "time_compute" | awk '{print $2}')
    fi
    
    # Store baseline (P=1 sequential)
    if [ "$P" -eq 1 ]; then
        BASELINE_TIME=$COMPUTE_TIME
        SPEEDUP="1.00"
        EFFICIENCY="100%"
    else
        # Calculate speedup = T1 / Tp
        SPEEDUP=$(echo "scale=2; $BASELINE_TIME / $COMPUTE_TIME" | bc)
        # Calculate efficiency = speedup / P * 100
        EFF_RAW=$(echo "scale=2; ($SPEEDUP / $P) * 100" | bc)
        EFFICIENCY="${EFF_RAW}%"
    fi
    
    # Print results
    printf "| %9d | %12s | %10s | %7s | %10s |\n" $P "${COMPUTE_TIME}s" "${TOTAL_TIME}s" "${SPEEDUP}x" "$EFFICIENCY"
    
    # Append to CSV
    echo "$P,$ITEMS,$DEPTH,$WIDTH,$COMPUTE_TIME,$TOTAL_TIME,$SPEEDUP,$EFF_RAW" >> $RESULTS_CSV
done

echo ""
echo -e "${GREEN}Results saved to: $RESULTS_CSV${NC}"
echo -e "${YELLOW}========================================${NC}"
echo -e "${YELLOW}Benchmark Complete!${NC}"
echo -e "${YELLOW}========================================${NC}"
