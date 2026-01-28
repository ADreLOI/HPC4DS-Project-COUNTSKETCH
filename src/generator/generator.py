import numpy as np
import os

# 1. Define the project root (two levels up from this file)
project_root = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
target_folder = os.path.join(project_root, "datasets")

# 2. Check if the directory exists; if not, create it
if not os.path.exists(target_folder):
    print(f"Directory {target_folder} not found. Creating it...")
    os.makedirs(target_folder)

def generate_zipf_data(filename, num_elements, num_unique_words, zipf_exponent):
    """
    Generating a text file with strings following a Zipf distribution.
    
    - num_elements: Total number of strings in the file (e.g., 1,000,000)
    - num_unique_words: Vocabulary size (e.g., 10,000)
    - zipf_exponent: Skewness parameter (>1). The higher it is, the more few elements dominate.
    """
    print(f"Generating {num_elements} elements...")
   
    data = np.random.zipf(zipf_exponent, num_elements)

    # Map values to the range [1, num_unique_words]
    data = np.mod(data, num_unique_words) + 1
    
    with open(filename, 'w') as f:
        for value in data:
            f.write(f"item_{value}\n")
            
    print(f"File '{filename}' generated successfully.")

# Parameters
TOTAL_ITEMS = 100      # 1 million strings
VOCAB_SIZE = 50000        # 50 thousand possible unique words
SKEW = 1.2                # Typical value for natural language

FILENAME = os.path.join(target_folder, f"input_data_{TOTAL_ITEMS}.txt")
generate_zipf_data(FILENAME, TOTAL_ITEMS, VOCAB_SIZE, SKEW)