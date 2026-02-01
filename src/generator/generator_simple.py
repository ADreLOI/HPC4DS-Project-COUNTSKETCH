#!/usr/bin/env python
"""
Simple Zipf Data Generator - Compatible with Python 2 and 3
Generates items following a Zipf (power-law) distribution.
"""
from __future__ import print_function
import random
import sys
import os

def main():
    # Parse command line arguments manually (no argparse for max compatibility)
    items = 8388608  # default 8M
    output = "datasets/input_data_8M.txt"
    vocab = 100000
    skew = 1.5
    seed = 42
    
    # Simple argument parsing
    i = 1
    while i < len(sys.argv):
        if sys.argv[i] == "--items" and i + 1 < len(sys.argv):
            items = int(sys.argv[i + 1])
            i += 2
        elif sys.argv[i] == "--output" and i + 1 < len(sys.argv):
            output = sys.argv[i + 1]
            i += 2
        elif sys.argv[i] == "--vocab" and i + 1 < len(sys.argv):
            vocab = int(sys.argv[i + 1])
            i += 2
        elif sys.argv[i] == "--skew" and i + 1 < len(sys.argv):
            skew = float(sys.argv[i + 1])
            i += 2
        elif sys.argv[i] == "--seed" and i + 1 < len(sys.argv):
            seed = int(sys.argv[i + 1])
            i += 2
        elif sys.argv[i] == "--help" or sys.argv[i] == "-h":
            print("Usage: python generator_simple.py [options]")
            print("  --items N     Number of items to generate (default: 8388608)")
            print("  --output FILE Output file path")
            print("  --vocab N     Vocabulary size (default: 100000)")
            print("  --skew F      Zipf skew parameter (default: 1.5)")
            print("  --seed N      Random seed (default: 42)")
            sys.exit(0)
        else:
            i += 1
    
    print("Generating %d items with Zipf distribution..." % items)
    print("  Vocabulary: %d, Skew: %.2f, Seed: %d" % (vocab, skew, seed))
    print("  Output: %s" % output)
    
    # Set random seed for reproducibility
    random.seed(seed)
    
    # Generate Zipf weights (power-law distribution)
    # P(k) proportional to 1/k^s where s is the skew
    weights = [1.0 / (k ** skew) for k in range(1, vocab + 1)]
    total_weight = sum(weights)
    cumulative = []
    running = 0.0
    for w in weights:
        running += w / total_weight
        cumulative.append(running)
    
    # Create output directory if needed
    output_dir = os.path.dirname(output)
    if output_dir and not os.path.exists(output_dir):
        os.makedirs(output_dir)
    
    # Generate items using weighted random selection
    print("Writing items to file...")
    batch_size = 100000
    with open(output, 'w') as f:
        for batch_start in range(0, items, batch_size):
            batch_end = min(batch_start + batch_size, items)
            for _ in range(batch_end - batch_start):
                r = random.random()
                # Binary search for the item
                lo, hi = 0, vocab - 1
                while lo < hi:
                    mid = (lo + hi) // 2
                    if cumulative[mid] < r:
                        lo = mid + 1
                    else:
                        hi = mid
                # Write item (as "item_XXXX" format)
                f.write("item_%d\n" % (lo + 1))
            
            # Progress update
            progress = 100.0 * batch_end / items
            print("  Progress: %.1f%% (%d/%d)" % (progress, batch_end, items))
    
    print("Done! Generated %d items in %s" % (items, output))

if __name__ == "__main__":
    main()
