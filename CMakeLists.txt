cmake_minimum_required(VERSION 3.10)
project(HPC_Project C)

set(CMAKE_C_STANDARD 99)

# --- FIX PER MAC (Apple Silicon) ---
if(APPLE)
    set(CMAKE_PREFIX_PATH "/opt/homebrew/opt/libomp" ${CMAKE_PREFIX_PATH})
    include_directories("/opt/homebrew/opt/libomp/include")
    link_directories("/opt/homebrew/opt/libomp/lib")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Xpreprocessor -fopenmp")
endif()

# Trova i pacchetti necessari
find_package(OpenMP REQUIRED)
find_package(MPI REQUIRED)

include_directories(${CMAKE_SOURCE_DIR}/src/include)

# =========================================================
# 1. VERSIONE SEQUENZIALE
# =========================================================
# Cerco file .c sia come file singolo nella root di src, sia in una sottocartella
file(GLOB SEQ_SRC "src/*.c")
if(SEQ_SRC)
    add_executable(app_seq ${SEQ_SRC})
    # Link alla libreria matematica se serve (-lm)
    target_link_libraries(app_seq PRIVATE MPI::MPI_C)
    target_link_libraries(app_seq PRIVATE m)
    
    # Se il sequenziale usa OpenMP (es. per testare solo le pragma senza MPI)
    if(OpenMP_C_FOUND)
        if(APPLE)
            target_link_libraries(app_seq PRIVATE "/opt/homebrew/opt/libomp/lib/libomp.dylib")
        else()
            target_link_libraries(app_seq PRIVATE OpenMP::OpenMP_C)
        endif()
    endif()
endif()

# =========================================================
# 2. VERSIONE SOLO MPI
# =========================================================
file(GLOB MPI_SRC "src/parallel_mpi/*.c")

if(MPI_SRC)
    add_executable(app_mpi ${MPI_SRC})
    target_link_libraries(app_mpi PRIVATE MPI::MPI_C)
    target_link_libraries(app_mpi PRIVATE m)
endif()

# =========================================================
# 3. VERSIONE IBRIDA (MPI + OpenMP)
# =========================================================
file(GLOB HYBRID_SRC "src/parallel_mpi_openMP/*.c")

if(HYBRID_SRC)
    add_executable(app_hybrid ${HYBRID_SRC})
    target_link_libraries(app_hybrid PRIVATE MPI::MPI_C)
    target_link_libraries(app_hybrid PRIVATE m)

    if(OpenMP_C_FOUND)
        if(APPLE)
            target_link_libraries(app_hybrid PRIVATE "/opt/homebrew/opt/libomp/lib/libomp.dylib")
        else()
            target_link_libraries(app_hybrid PRIVATE OpenMP::OpenMP_C)
        endif()
    endif()
endif()