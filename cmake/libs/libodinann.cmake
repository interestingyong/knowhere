# OdinANN CMake configuration
add_definitions(-DKNOWHERE_WITH_ODINANN)

# Check if liburing is built locally
set(LIBURING_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/OdinANN/third_party/liburing)
set(LIBURING_INSTALL_DIR ${LIBURING_ROOT}/install)

# Try to compile and install liburing if not already done
if(NOT EXISTS ${LIBURING_INSTALL_DIR})
  message(STATUS "liburing not found, attempting to build...")
  execute_process(
    COMMAND bash -c "cd ${LIBURING_ROOT} && ./configure --prefix=${LIBURING_INSTALL_DIR} --libdir=${LIBURING_INSTALL_DIR}/lib && make -j && make install"
    RESULT_VARIABLE LIBURING_BUILD_RESULT
    OUTPUT_VARIABLE LIBURING_BUILD_OUTPUT
    ERROR_VARIABLE LIBURING_BUILD_ERROR
  )
  
  if(NOT LIBURING_BUILD_RESULT EQUAL 0)
    message(FATAL_ERROR "Failed to build liburing: ${LIBURING_BUILD_ERROR}")
  endif()
endif()

include_directories(thirdparty/OdinANN/include)
include_directories(${LIBURING_INSTALL_DIR}/include)

# Collect OdinANN source files
set(ODINANN_SOURCES
    thirdparty/OdinANN/src/utils/distance.cpp
    thirdparty/OdinANN/src/index.cpp
    thirdparty/OdinANN/src/utils/aux_utils.cpp
    thirdparty/OdinANN/src/utils/linux_aligned_file_reader.cpp
    thirdparty/OdinANN/src/utils/math_utils.cpp
    thirdparty/OdinANN/src/utils/partition_and_pq.cpp
    thirdparty/OdinANN/src/search/beam_search.cpp
    thirdparty/OdinANN/src/search/pipe_search.cpp
)

# Create OdinANN static library
add_library(odinann SHARED ${ODINANN_SOURCES})

# Set compile options for OdinANN
target_compile_options(
    odinann PRIVATE
    -fopenmp
    -fopenmp-simd
    -Wall
    -Wextra
    -Wfatal-errors
    -Wno-class-memaccess
    -Wno-unused-variable
    -Wno-unused-parameter
    -Wno-unused-but-set-variable
    -g -O3 -march=native -mtune=native -ftree-vectorize -funroll-loops
)

# Link required libraries
find_library(LIBURING_LIBRARY NAMES uring PATHS ${LIBURING_INSTALL_DIR}/lib NO_DEFAULT_PATH)
if(NOT LIBURING_LIBRARY)
    message(FATAL_ERROR "liburing library not found in ${LIBURING_INSTALL_DIR}/lib")
endif()

target_link_libraries(
    odinann
    PUBLIC OpenMP::OpenMP_CXX
    PUBLIC ${LIBURING_LIBRARY}
)

# Add to knowhere linker libs
list(APPEND KNOWHERE_LINKER_LIBS odinann)