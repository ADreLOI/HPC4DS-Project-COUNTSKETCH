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
CSV_FILE="${RESULTS_DIR}/weak_scaling_hybrid_${TOTAL_LINES}.csv"

echo "Processes,N_Threads,Total_Lines,Serial_Time,Compute_Time,Compute_Communication_Time,Weak_Scaling_Compute,Weak_Scaling_Communication" > "$CSV_FILE"


# --- TEST WEAK SCALING ---
echo -e "\nStarting Weak Scaling Tests..."
echo "| Processes | N_Threads | Total Lines | Serial Time | Compute Time | Compute + Communication Time | Weak Scaling Compute | Weak Scaling Communication |"
echo "|-----------|-----------|-------------|-------------|--------------|------------------------------|----------------------|----------------------------|"

# In Weak Scaling, keep threads fixed (e.g., 8 as in your code)
for np in 2 4 8 16 32 64
do
    # scaling_mode = 1 (Weak)
    mpirun -np $np $EXE $TRIALS $BUCKETS $INPUT_FILE 8 1 | \
        grep "|" | grep -v "Processes" | grep -v -- "---"
done

echo "CSV results saved to: $CSV_FILE"
echo "Weak scaling study completed successfully."