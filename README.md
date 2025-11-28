# Parallel Count Sketch

MPI and MPI+OpenMP implementations of Sparsifying Count Sketch with Fragment Disaggregation strategies for high-performance frequency estimation in streaming data.

## Project Overview

This project implements and compares two parallel strategies:
1. **Sparse MPI Distribution** (Verma et al., 2024)
2. **Fragment Disaggregation with MPI+OpenMP** (March 2025)

**Course**: High Performance Computing for Data Science  
**Institution**: University of Trento, DISI  
**Academic Year**: 2025-2026  

## Team Members
- Andrea Lo Iacono (andrea.loiacono@studenti.unitn.it)
- Matthew De Marco (matthew.demarco@studenti.unitn.it)

## Repository Structure

src/ Source code
sequential/ Baseline implementation
mpi_sparse/ Strategy 1 (Sparse MPI)
mpi_openmp_hybrid/ Strategy 2 (MPI+OpenMP)
test/ Unit tests
data/ Test datasets
scripts/ Build and benchmark scripts
results/ Experimental results
docs/ Documentation

## Building

cd scripts
./build.sh

## References

1. Verma et al. (2024). "Sparsifying Count Sketch"
2. Sketch Disaggregation (2025). arXiv:2503.13515
3. Higgins et al. (2025). "GPU CountSketch Implementation"

## License

MIT License - see LICENSE file