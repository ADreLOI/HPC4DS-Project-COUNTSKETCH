import pandas as pd
import matplotlib.pyplot as plt
import os

def generate_strong_scaling_plots(csv_file):
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
    plt.savefig('strong_scaling_time.png', dpi=300)
    print("Saved: strong_scaling_time.png")
    plt.close()

    # 2. Total Speedup Plot
    plt.figure(figsize=(10, 6))
    for proc in sorted(df['Processes'].unique()):
        subset = df[df['Processes'] == proc]
        plt.plot(subset['N_Threads'], subset['Speedup_Compute'], marker='o', label=f'Processes={proc}')
    
    plt.title('Strong Scaling: Total Speedup (Compute)', fontsize=14)
    plt.xlabel('Number of Threads', fontsize=12)
    plt.ylabel('Speedup Factor', fontsize=12)
    plt.xscale('log', base=2)
    plt.xticks(df['N_Threads'].unique(), df['N_Threads'].unique())
    plt.legend(title='MPI Ranks')
    plt.grid(True, which="both", ls="-", alpha=0.5)
    plt.savefig('strong_scaling_speedup_compute.png', dpi=300)
    print("Saved: strong_scaling_speedup_compute.png")
    plt.close()

def generate_weak_scaling_plots(csv_file):
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
    plt.savefig('weak_scaling_times.png', dpi=300)
    print("Saved: weak_scaling_times.png")
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
    plt.ylim(0, 1.6)
    plt.legend()
    plt.grid(True, which="both", ls="-", alpha=0.5)
    plt.savefig('weak_scaling_efficiency_compute.png', dpi=300)
    print("Saved: weak_scaling_efficiency_compute.png")
    plt.close()

if __name__ == "__main__":
    # Update these filenames to match your files
    performance_data = 'results/performance_hybrid_8388608.csv'
    weak_scaling_data = 'results/weak_scaling_hybrid_8388608.csv'

    print("--- Generating Scaling Plots ---")
    generate_strong_scaling_plots(performance_data)
    generate_weak_scaling_plots(weak_scaling_data)
    print("--- Done ---")