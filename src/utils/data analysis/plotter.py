import pandas as pd
import matplotlib.pyplot as plt
import os
import re

def extract_total_lines_from_filename(csv_file):
    """Extracts the trailing integer from filenames like performance_hybrid_8388608.csv."""
    base = os.path.basename(csv_file)
    match = re.search(r"_(\d+)\.csv$", base)
    return int(match.group(1)) if match else None

def generate_strong_scaling_plots(csv_file, total_lines):
    """Generates plots for Strong Scaling (Performance)."""
    if not os.path.exists(csv_file):
        print(f"Warning: {csv_file} not found.")
        return

    df = pd.read_csv(csv_file)
    df = df.sort_values(['Processes', 'N_Threads'])
    
    # 1. Total Execution Time Plot
    plt.figure(figsize=(10, 6))
    for proc in sorted(df['Processes'].unique()):
        subset = df[df['Processes'] == proc]
        plt.plot(subset['N_Threads'], subset['Total_Time'], marker='o', label=f'Processes={proc}')
    
    plt.title('Strong Scaling: Total Execution Time vs. Threads', fontsize=14)
    plt.xlabel('Number of Threads', fontsize=12)
    plt.ylabel('Total Time (seconds)', fontsize=12)
    plt.xscale('log', base=2)
    plt.yscale('log')
    plt.xticks(df['N_Threads'].unique(), df['N_Threads'].unique())
    plt.legend(title='MPI Ranks')
    plt.grid(True, which="both", ls="-", alpha=0.5)
    plt.savefig(f'strong_scaling_time_{total_lines}.png', dpi=300)
    print(f"Saved: strong_scaling_time_{total_lines}.png")
    plt.close()

    # 2. Total Speedup Plot
    plt.figure(figsize=(10, 6))
    for proc in sorted(df['Processes'].unique()):
        subset = df[df['Processes'] == proc]
        plt.plot(subset['N_Threads'], subset['Speedup_Comm'], marker='o', label=f'Processes={proc}')
    
    plt.title('Strong Scaling: Total Speedup (Compute + Communication)', fontsize=14)
    plt.xlabel('Number of Threads', fontsize=12)
    plt.ylabel('Speedup Factor', fontsize=12)
    plt.xscale('log', base=2)
    plt.xticks(df['N_Threads'].unique(), df['N_Threads'].unique())
    plt.legend(title='MPI Ranks')
    plt.grid(True, which="both", ls="-", alpha=0.5)
    plt.savefig(f'strong_scaling_speedup_{total_lines}.png', dpi=300)
    print(f"Saved: strong_scaling_speedup_{total_lines}.png")
    plt.close()

def generate_weak_scaling_plots(csv_file, total_lines):
    """Generates plots for Weak Scaling."""
    if not os.path.exists(csv_file):
        print(f"Warning: {csv_file} not found.")
        return

    df = pd.read_csv(csv_file)
    df = df.sort_values('Processes')

    # 3. Weak Scaling Execution Times
    plt.figure(figsize=(10, 6))
    plt.plot(df['Processes'], df['Compute_Time'], marker='o', label='Pure Compute (Hashing)')
    plt.plot(df['Processes'], df['Compute_Communication_Time'], marker='s', label='Total (Scatter + Compute + Reduce)')
    
    plt.title('Weak Scaling: Execution Times (Constant Work/Rank)', fontsize=14)
    plt.xlabel('Number of MPI Processes', fontsize=12)
    plt.ylabel('Time (seconds)', fontsize=12)
    plt.xscale('log', base=2)
    plt.xticks(df['Processes'], df['Processes'])
    plt.legend()
    plt.grid(True, which="both", ls="-", alpha=0.5)
    plt.savefig(f'weak_scaling_times_{total_lines}.png', dpi=300)
    print(f"Saved: weak_scaling_times_{total_lines}.png")
    plt.close()

    # 4. Weak Scaling Efficiency Plot
    plt.figure(figsize=(10, 6))
    plt.plot(df['Processes'], df['Weak_Scaling_Compute'], marker='o', color='green', label='Measured Weak Efficiency')
    plt.axhline(y=1.0, color='red', linestyle='--', label='Ideal Scaling (1.0)')
    
    plt.title('Weak Scaling: Efficiency vs. Processes', fontsize=14)
    plt.xlabel('Number of MPI Processes', fontsize=12)
    plt.ylabel('Efficiency Value (T1 / Tn)', fontsize=12)
    plt.xscale('log', base=2)
    plt.xticks(df['Processes'], df['Processes'])
    plt.ylim(0, 1.8)
    plt.legend()
    plt.grid(True, which="both", ls="-", alpha=0.5)
    plt.savefig(f'weak_scaling_efficiency_compute_{total_lines}.png', dpi=300)
    print(f"Saved: weak_scaling_efficiency_compute_{total_lines}.png")
    plt.close()

def plot_strong_scaling_comparison(csv_files):
    """
    Compares Average Speedup across different dataset sizes.
    Groups by 'Processes' and averages values across different thread counts.
    """
    plt.figure(figsize=(10, 6))

    for csv in csv_files:
        if not os.path.exists(csv):
            print(f"Skipping: {csv} (not found)")
            continue
            
        df = pd.read_csv(csv)
        
        # --- KEY MODIFICATION: Group and Average ---
        # We group by 'Processes' and calculate the mean for each group
        # This gives us one value per process count (2, 4, 8, 16, 32, 64)
        df_avg = df.groupby('Processes')['Speedup_Compute'].mean().reset_index()
        df_avg = df_avg.sort_values('Processes')
        
        size_label = extract_total_lines_from_filename(csv)
        
        # Plotting the averaged Speedup
        plt.plot(df_avg['Processes'], df_avg['Speedup_Compute'], 
                    marker='o', linestyle='-', label=f'Size: {size_label} (Avg)')

    # --- Corrected Ideal Line ---
    # In Strong Scaling, Ideal Speedup = Number of Resources
    max_proc = df_avg['Processes'].max()
    min_proc = df_avg['Processes'].min()
    plt.plot([min_proc, max_proc], [min_proc, max_proc], 
                'r--', alpha=0.8, label='Ideal Linear Speedup')

    # Formatting
    plt.xscale('log', base=2)
    plt.yscale('log', base=10) # Log scale often helps visualize scaling better
    plt.xlabel('Number of MPI Processes', fontsize=12)
    plt.ylabel('Average Speedup Factor', fontsize=12)
    plt.title('Strong Scaling: Mean Speedup across Processes', fontsize=14)

    # Set xticks to match your actual process counts
    plt.xticks(df_avg['Processes'].unique(), df_avg['Processes'].unique())

    plt.legend()
    plt.grid(True, which="both", ls="-", alpha=0.3)

    plt.savefig("strong_comparison_compute.png", dpi=300)
    plt.close()
   

def plot_weak_scaling_comparison(csv_files, output_name='weak_comparison.png'):
    """
    Compares Efficiency across different workloads per rank.
    Expects columns: Processes, Weak_Scaling_Communication
    """
    plt.figure(figsize=(10, 6))
    
    for csv in csv_files:
        df = pd.read_csv(csv)
        df = df.sort_values('Processes')
        
        # Calculate workload per rank to create a better label
        workload = df['Total_Lines'].iloc[0] // df['Processes'].iloc[0]
        
        plt.plot(df['Processes'], df['Weak_Scaling_Communication'], marker='s', label=f'Workload: {workload} lines/rank')
    
    plt.axhline(y=1.0, color='r', linestyle='--', alpha=0.7, label='Ideal Efficiency')
    plt.xscale('log', base=2)
    plt.xlabel('Number of MPI Processes')
    plt.ylabel('Efficiency (T_serial / T_parallel)')
    plt.title('Weak Scaling Comparison: Efficiency vs. Process Count')
    plt.xticks(df['Processes'].unique(), df['Processes'].unique())
    plt.ylim(0, 1.8) # Adjust if you have super-linear speedup
    plt.legend()
    plt.grid(True, which="both", ls="-", alpha=0.3)
    plt.savefig(output_name, dpi=300)
    plt.close()

# --- Example Usage


if __name__ == "__main__":
    # Update these filenames to match your files
    performance_data = 'results/performance_hybrid_1048576.csv'
    weak_scaling_data = 'results/weak_scaling_hybrid_1048576.csv'

    performance_data_1M = 'results/performance_hybrid_1048576.csv'
    performance_data_0_5M = 'results/performance_hybrid_524288.csv'
    performance_data_8M = 'results/performance_hybrid_8388608.csv'

    weak_scaling_data_1M = 'results/weak_scaling_hybrid_1048576.csv'
    weak_scaling_data_0_5M = 'results/weak_scaling_hybrid_524288.csv'
    weak_scaling_data_8M = 'results/weak_scaling_hybrid_8388608.csv'

    total_lines = extract_total_lines_from_filename(performance_data)
    if total_lines is not None:
        print(f"Detected total lines: {total_lines}")

    print("--- Generating Scaling Plots ---")
    generate_strong_scaling_plots(performance_data, total_lines)
    generate_weak_scaling_plots(weak_scaling_data, total_lines)
    #plot_strong_scaling_comparison([performance_data_0_5M, performance_data_1M, performance_data_8M])
    #plot_weak_scaling_comparison([weak_scaling_data_0_5M, weak_scaling_data_1M, weak_scaling_data_8M], output_name='weak_scaling_comparison.png')
    print("--- Done ---")
