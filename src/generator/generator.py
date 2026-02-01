#!/usr/bin/env python3
"""
============================================================================
DATASET GENERATOR - Zipf Distribution for Count Sketch Testing
============================================================================

Generates datasets with Zipf-distributed item frequencies, which is realistic
for many real-world scenarios (word frequencies, web page visits, etc.).

USAGE:
    python generator.py [options]

OPTIONS:
    --items N       Total number of items to generate (default: 100000)
    --vocab N       Vocabulary size / number of unique items (default: 50000)
    --skew FLOAT    Zipf exponent, higher = more skewed (default: 1.2)
    --output FILE   Output file path (default: auto-generated in datasets/)
    --truth FILE    Output ground truth frequencies to this file
    --batch         Generate multiple sizes for scalability testing

EXAMPLES:
    # Generate 1 million items
    python generator.py --items 1000000 --output datasets/data_1M.txt
    
    # Generate datasets for scalability testing
    python generator.py --batch
    
    # Generate with ground truth for accuracy validation
    python generator.py --items 10000 --truth results/truth.csv

============================================================================
"""

import numpy as np
import os
import argparse
from collections import Counter
from typing import Dict, List, Tuple

def parse_args() -> argparse.Namespace:
    """Parse command-line arguments."""
    parser = argparse.ArgumentParser(
        description="Generate Zipf-distributed datasets for Count Sketch testing",
        formatter_class=argparse.RawDescriptionHelpFormatter
    )
    
    parser.add_argument(
        "--items", "-n", 
        type=int, 
        default=100000,
        help="Total number of items to generate (default: 100000)"
    )
    
    parser.add_argument(
        "--vocab", "-v", 
        type=int, 
        default=50000,
        help="Vocabulary size / number of unique items (default: 50000)"
    )
    
    parser.add_argument(
        "--skew", "-s", 
        type=float, 
        default=1.2,
        help="Zipf exponent (>1). Higher = more skewed distribution (default: 1.2)"
    )
    
    parser.add_argument(
        "--output", "-o", 
        type=str, 
        default=None,
        help="Output file path (default: datasets/input_data_<items>.txt)"
    )
    
    parser.add_argument(
        "--truth", "-t", 
        type=str, 
        default=None,
        help="Output ground truth frequencies to this CSV file"
    )
    
    parser.add_argument(
        "--batch", "-b", 
        action="store_true",
        help="Generate multiple dataset sizes for scalability testing"
    )
    
    parser.add_argument(
        "--seed", 
        type=int, 
        default=42,
        help="Random seed for reproducibility (default: 42)"
    )
    
    return parser.parse_args()


def get_project_paths() -> Tuple[str, str]:
    """Get project root and datasets directory paths."""
    # This script is in src/generator/
    script_dir = os.path.dirname(os.path.abspath(__file__))
    project_root = os.path.abspath(os.path.join(script_dir, "..", ".."))
    datasets_dir = os.path.join(project_root, "datasets")
    results_dir = os.path.join(project_root, "results")
    
    # Create directories if they don't exist
    os.makedirs(datasets_dir, exist_ok=True)
    os.makedirs(results_dir, exist_ok=True)
    
    return datasets_dir, results_dir


def generate_zipf_data(
    num_items: int,
    vocab_size: int,
    skew: float,
    seed: int = 42
) -> List[str]:
    """
    Generate a list of items following Zipf distribution.
    
    Zipf's law: The frequency of an item is inversely proportional to its rank.
    P(rank=k) ∝ 1/k^s where s is the skew parameter.
    
    Args:
        num_items: Total number of items to generate
        vocab_size: Number of unique item types
        skew: Zipf exponent (typically 1.0-2.0, higher = more skewed)
        seed: Random seed for reproducibility
    
    Returns:
        List of item strings
    """
    print(f"Generating {num_items:,} items with Zipf distribution...")
    print(f"  Vocabulary size: {vocab_size:,}")
    print(f"  Skew parameter: {skew}")
    
    # Set random seed for reproducibility
    np.random.seed(seed)
    
    # Generate Zipf-distributed values
    # numpy.random.zipf generates values >= 1
    raw_data = np.random.zipf(skew, num_items)
    
    # Map to vocabulary range [1, vocab_size]
    # Using modulo to wrap large values
    mapped_data = (raw_data % vocab_size) + 1
    
    # Convert to item strings
    items = [f"item_{val}" for val in mapped_data]
    
    return items


def compute_ground_truth(items: List[str]) -> Dict[str, int]:
    """
    Compute exact frequency counts for all items.
    
    This serves as ground truth for validating Count Sketch accuracy.
    
    Args:
        items: List of item strings
    
    Returns:
        Dictionary mapping item -> exact count
    """
    return dict(Counter(items))


def write_items_to_file(items: List[str], filepath: str) -> None:
    """Write items to file, one per line."""
    with open(filepath, 'w') as f:
        for item in items:
            f.write(f"{item}\n")
    print(f"Written {len(items):,} items to '{filepath}'")


def write_ground_truth_csv(frequencies: Dict[str, int], filepath: str) -> None:
    """
    Write ground truth frequencies to CSV file.
    
    CSV format: item,frequency
    Sorted by frequency (descending) for easy inspection.
    """
    # Sort by frequency descending
    sorted_items = sorted(frequencies.items(), key=lambda x: -x[1])
    
    with open(filepath, 'w') as f:
        f.write("item,frequency\n")
        for item, freq in sorted_items:
            f.write(f"{item},{freq}\n")
    
    print(f"Written ground truth ({len(frequencies):,} unique items) to '{filepath}'")
    
    # Print summary statistics
    freqs = list(frequencies.values())
    print(f"  Max frequency: {max(freqs):,}")
    print(f"  Min frequency: {min(freqs):,}")
    print(f"  Mean frequency: {np.mean(freqs):.2f}")
    print(f"  Median frequency: {np.median(freqs):.2f}")


def generate_batch_datasets(datasets_dir: str, results_dir: str, seed: int = 42) -> None:
    """
    Generate multiple dataset sizes for scalability testing.
    
    Generates:
    - Small:   10,000 items (for quick tests)
    - Medium:  100,000 items
    - Large:   1,000,000 items
    - XLarge:  10,000,000 items (for cluster)
    """
    # Dataset sizes for scaling tests (as discussed with your teammate)
    # From 10K to 1M items - more items = better MPI speedup
    sizes = [
        (10_000, "10K"),
        (50_000, "50K"),
        (100_000, "100K"),
        (500_000, "500K"),
        (1_000_000, "1M"),
    ]
    
    print("\n" + "="*60)
    print("BATCH DATASET GENERATION")
    print("="*60)
    
    for num_items, label in sizes:
        print(f"\n--- Generating {label} dataset ---")
        
        # Generate items
        items = generate_zipf_data(
            num_items=num_items,
            vocab_size=min(50_000, num_items // 2),  # Reasonable vocab size
            skew=1.2,
            seed=seed
        )
        
        # Write data file
        data_file = os.path.join(datasets_dir, f"input_data_{label}.txt")
        write_items_to_file(items, data_file)
        
        # Write ground truth for the smallest datasets (for validation)
        if num_items <= 100_000:
            truth_file = os.path.join(results_dir, f"truth_{label}.csv")
            ground_truth = compute_ground_truth(items)
            write_ground_truth_csv(ground_truth, truth_file)
    
    print("\n" + "="*60)
    print("Batch generation complete!")
    print("="*60)


def main():
    """Main entry point."""
    args = parse_args()
    datasets_dir, results_dir = get_project_paths()
    
    if args.batch:
        # Generate multiple sizes for scalability testing
        generate_batch_datasets(datasets_dir, results_dir, args.seed)
        return
    
    # Generate single dataset
    items = generate_zipf_data(
        num_items=args.items,
        vocab_size=args.vocab,
        skew=args.skew,
        seed=args.seed
    )
    
    # Determine output path
    if args.output:
        output_path = args.output
    else:
        output_path = os.path.join(datasets_dir, f"input_data_{args.items}.txt")
    
    # Write items to file
    write_items_to_file(items, output_path)
    
    # Write ground truth if requested
    if args.truth:
        ground_truth = compute_ground_truth(items)
        write_ground_truth_csv(ground_truth, args.truth)
    
    print("\nDataset generation complete!")


if __name__ == "__main__":
    main()