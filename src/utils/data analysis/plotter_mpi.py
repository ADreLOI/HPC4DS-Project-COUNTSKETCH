#!/usr/bin/env python3
"""
MPI Count Sketch - Plotter Script

Generates charts for MPI benchmarks:
  1. strong_scaling_speedup_compute.png
  2. strong_scaling_speedup.png (with communication)
  3. strong_scaling_time.png
  4. weak_scaling_efficiency_compute.png
  5. weak_scaling_efficiency.png (with communication)
  6. weak_scaling_times.png
  7. strong_comparison_mpi.png (3 sizes comparison)
  8. weak_comparison_mpi.png (optional)

Usage:
    python src/utils/data\ analysis/plotter_mpi.py
"""

import pandas as pd
import matplotlib.pyplot as plt
import os
import re

# Output directory for charts
CHARTS_DIR = "charts"
os.makedirs(CHARTS_DIR, exist_ok=True)


def extract_lines_from_filename(csv_file):
    """Extract number of lines from filename like performance_mpi_8388608.csv"""
    match = re.search(r"_(\d+)\.csv$", os.path.basename(csv_file))
    return int(match.group(1)) if match else None


def generate_strong_scaling_plots(csv_file):
    """
    Generate Strong Scaling charts for MPI.
    For pure MPI, we only have one data point per process count (N_Threads=1).
    """
    if not os.path.exists(csv_file):
        print(f"Warning: {csv_file} not found.")
        return
    
    df = pd.read_csv(csv_file)
    df = df.sort_values('Processes')
    total_lines = extract_lines_from_filename(csv_file)
    
    print(f"Generating strong scaling plots for {total_lines} items...")
    
    # 1. Strong Scaling Speedup (Compute Only)
    plt.figure(figsize=(10, 6))
    plt.plot(df['Processes'], df['Speedup_Compute'], 
             marker='o', linewidth=2, markersize=8, color='blue', label='MPI Speedup')
    
    # Ideal line
    max_p = df['Processes'].max()
    min_p = df['Processes'].min()
    plt.plot([min_p, max_p], [min_p, max_p], 
             'r--', linewidth=1.5, alpha=0.7, label='Ideal Linear Speedup')
    
    plt.title(f'Strong Scaling: Speedup (Compute Only) - {total_lines} items', fontsize=14)
    plt.xlabel('Number of MPI Processes', fontsize=12)
    plt.ylabel('Speedup Factor', fontsize=12)
    plt.xscale('log', base=2)
    plt.xticks(df['Processes'], df['Processes'])
    plt.legend()
    plt.grid(True, which="both", ls="-", alpha=0.5)
    plt.tight_layout()
    plt.savefig(f'{CHARTS_DIR}/strong_scaling_speedup_compute.png', dpi=300)
    print(f"  Saved: strong_scaling_speedup_compute.png")
    plt.close()
    
    # 2. Strong Scaling Speedup (With Communication)
    plt.figure(figsize=(10, 6))
    plt.plot(df['Processes'], df['Speedup_Comm'], 
             marker='s', linewidth=2, markersize=8, color='green', label='MPI Speedup (with comm)')
    plt.plot([min_p, max_p], [min_p, max_p], 
             'r--', linewidth=1.5, alpha=0.7, label='Ideal Linear Speedup')
    
    plt.title(f'Strong Scaling: Speedup (With Communication) - {total_lines} items', fontsize=14)
    plt.xlabel('Number of MPI Processes', fontsize=12)
    plt.ylabel('Speedup Factor', fontsize=12)
    plt.xscale('log', base=2)
    plt.xticks(df['Processes'], df['Processes'])
    plt.legend()
    plt.grid(True, which="both", ls="-", alpha=0.5)
    plt.tight_layout()
    plt.savefig(f'{CHARTS_DIR}/strong_scaling_speedup.png', dpi=300)
    print(f"  Saved: strong_scaling_speedup.png")
    plt.close()
    
    # 3. Strong Scaling Execution Time
    plt.figure(figsize=(10, 6))
    plt.plot(df['Processes'], df['Compute_Time'], 
             marker='o', linewidth=2, markersize=8, color='blue', label='Compute Only')
    plt.plot(df['Processes'], df['Total_Time'], 
             marker='s', linewidth=2, markersize=8, color='green', label='Total (with comm)')
    
    plt.title(f'Strong Scaling: Execution Time - {total_lines} items', fontsize=14)
    plt.xlabel('Number of MPI Processes', fontsize=12)
    plt.ylabel('Time (seconds)', fontsize=12)
    plt.xscale('log', base=2)
    plt.yscale('log')
    plt.xticks(df['Processes'], df['Processes'])
    plt.legend()
    plt.grid(True, which="both", ls="-", alpha=0.5)
    plt.tight_layout()
    plt.savefig(f'{CHARTS_DIR}/strong_scaling_time.png', dpi=300)
    print(f"  Saved: strong_scaling_time.png")
    plt.close()


def generate_weak_scaling_plots(csv_file):
    """
    Generate Weak Scaling charts for MPI.
    
    Weak Scaling Efficiency = T1(n) / Tp(n)
    Where:
    - T1(n) = Time for 1 process with chunk size n
    - Tp(n) = Time for P processes, each with chunk size n
    
    Since Serial_Time in CSV is for FULL problem (chunk_size * P items),
    we need to divide by P to get the base chunk time.
    """
    if not os.path.exists(csv_file):
        print(f"Warning: {csv_file} not found.")
        return
    
    df = pd.read_csv(csv_file)
    df = df.sort_values('Processes')
    total_lines = extract_lines_from_filename(csv_file)
    
    print(f"Generating weak scaling plots for {total_lines} base items...")
    
    # CORRECT Weak Scaling Efficiency calculation:
    # T1_base = Serial_Time / P (time for 1 process to handle base chunk)
    # Efficiency = T1_base / Parallel_Time
    df['T1_base'] = df['Serial_Time'] / df['Processes']
    df['Efficiency_Compute_Correct'] = df['T1_base'] / df['Compute_Time']
    df['Efficiency_Comm_Correct'] = df['T1_base'] / df['Compute_Communication_Time']
    
    # 4. Weak Scaling Efficiency (Compute Only)
    plt.figure(figsize=(10, 6))
    plt.plot(df['Processes'], df['Efficiency_Compute_Correct'], 
             marker='o', linewidth=2, markersize=8, color='blue', label='Weak Efficiency (Compute)')
    plt.axhline(y=1.0, color='red', linestyle='--', linewidth=1.5, label='Ideal Scaling (1.0)')
    
    plt.title('Weak Scaling: Efficiency (Compute Only)', fontsize=14)
    plt.xlabel('Number of MPI Processes', fontsize=12)
    plt.ylabel('Efficiency (T1 / Tp)', fontsize=12)
    plt.xscale('log', base=2)
    plt.xticks(df['Processes'], df['Processes'])
    plt.ylim(0, 1.4)
    plt.legend()
    plt.grid(True, which="both", ls="-", alpha=0.5)
    plt.tight_layout()
    plt.savefig(f'{CHARTS_DIR}/weak_scaling_efficiency_compute.png', dpi=300)
    print(f"  Saved: weak_scaling_efficiency_compute.png")
    plt.close()
    
    # 5. Weak Scaling Efficiency (With Communication)
    plt.figure(figsize=(10, 6))
    plt.plot(df['Processes'], df['Efficiency_Comm_Correct'], 
             marker='o', linewidth=2, markersize=8, color='green', label='Measured Weak Efficiency')
    plt.axhline(y=1.0, color='red', linestyle='--', linewidth=1.5, label='Ideal Scaling (1.0)')
    
    plt.title('Weak Scaling: Efficiency vs. Processes', fontsize=14)
    plt.xlabel('Number of MPI Processes', fontsize=12)
    plt.ylabel('Efficiency Value (T1 / Tp)', fontsize=12)
    plt.xscale('log', base=2)
    plt.xticks(df['Processes'], df['Processes'])
    plt.ylim(0, 1.4)
    plt.legend()
    plt.grid(True, which="both", ls="-", alpha=0.5)
    plt.tight_layout()
    plt.savefig(f'{CHARTS_DIR}/weak_scaling_efficiency.png', dpi=300)
    print(f"  Saved: weak_scaling_efficiency.png")
    plt.close()
    
    # 6. Weak Scaling Execution Times
    plt.figure(figsize=(10, 6))
    plt.plot(df['Processes'], df['Compute_Time'], 
             marker='o', linewidth=2, markersize=8, color='blue', label='Pure Compute')
    plt.plot(df['Processes'], df['Compute_Communication_Time'], 
             marker='s', linewidth=2, markersize=8, color='green', label='Total (with comm)')
    
    plt.title('Weak Scaling: Execution Times', fontsize=14)
    plt.xlabel('Number of MPI Processes', fontsize=12)
    plt.ylabel('Time (seconds)', fontsize=12)
    plt.xscale('log', base=2)
    plt.xticks(df['Processes'], df['Processes'])
    plt.legend()
    plt.grid(True, which="both", ls="-", alpha=0.5)
    plt.tight_layout()
    plt.savefig(f'{CHARTS_DIR}/weak_scaling_times.png', dpi=300)
    print(f"  Saved: weak_scaling_times.png")
    plt.close()


def generate_comparison_plot(csv_files, output_name='strong_comparison_mpi.png'):
    """
    Generate comparison plot across multiple dataset sizes.
    """
    plt.figure(figsize=(10, 6))
    
    for csv in csv_files:
        if not os.path.exists(csv):
            print(f"Skipping: {csv} (not found)")
            continue
        
        df = pd.read_csv(csv)
        df = df.sort_values('Processes')
        
        size = extract_lines_from_filename(csv)
        size_label = f"{size//1000}K" if size < 1000000 else f"{size//1000000}M"
        
        plt.plot(df['Processes'], df['Speedup_Comm'], 
                 marker='o', linewidth=2, markersize=8, label=f'N = {size_label}')
    
    # Ideal line
    max_p = df['Processes'].max()
    min_p = df['Processes'].min()
    plt.plot([min_p, max_p], [min_p, max_p], 
             'r--', linewidth=1.5, alpha=0.7, label='Ideal')
    
    plt.title('Strong Scaling Comparison: Speedup vs. Dataset Size', fontsize=14)
    plt.xlabel('Number of MPI Processes', fontsize=12)
    plt.ylabel('Speedup Factor', fontsize=12)
    plt.xscale('log', base=2)
    plt.xticks(df['Processes'], df['Processes'])
    plt.legend()
    plt.grid(True, which="both", ls="-", alpha=0.5)
    plt.tight_layout()
    plt.savefig(f'{CHARTS_DIR}/{output_name}', dpi=300)
    print(f"  Saved: {output_name}")
    plt.close()


if __name__ == "__main__":
    print("=" * 60)
    print("MPI Count Sketch - Generating Benchmark Charts")
    print("=" * 60)
    
    # Main dataset (8M = 8388608)
    perf_8m = 'results/performance_mpi_8388608.csv'
    weak_8m = 'results/weak_scaling_mpi_8388608.csv'
    
    # Additional sizes for comparison
    perf_1m = 'results/performance_mpi_1048576.csv'
    perf_500k = 'results/performance_mpi_524288.csv'
    
    # Generate main charts from 8M dataset
    print("\n--- Strong Scaling Charts ---")
    generate_strong_scaling_plots(perf_8m)
    
    print("\n--- Weak Scaling Charts ---")
    generate_weak_scaling_plots(weak_8m)
    
    # Generate comparison chart
    print("\n--- Comparison Charts ---")
    generate_comparison_plot(
        [perf_500k, perf_1m, perf_8m],
        output_name='strong_comparison_mpi.png'
    )
    
    print("\n" + "=" * 60)
    print("Chart generation complete!")
    print(f"Charts saved to: {CHARTS_DIR}/")
    print("=" * 60)
