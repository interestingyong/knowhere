// ODINANN BUILD INTERFACE REFACTORING - IMPLEMENTATION CODE
// ===========================================================
// 
// This file shows the exact code changes needed in thirdparty/OdinANN
// to make it compatible with Knowhere's expectations.

/*
 * FILE: thirdparty/OdinANN/include/aux_utils.h
 * ============================================
 * 
 * ADD these new structures and function declarations:
 */

#pragma once
#include <chrono>
#include <string>
#include <thread>
#include "parameters.h"

namespace pipeann {

// ============================================================================
// NEW: Structured build configuration
// ============================================================================

struct DiskIndexBuildConfig {
    std::string data_path;              // Input: path to raw data binary file
    std::string index_prefix;           // Output: prefix for index files
    uint32_t max_degree;                // R: max graph degree (60-150 typical)
    uint32_t search_list_size;          // L: search list size during build (75-200)
    uint32_t pq_dims;                   // PQ compression dims (0=no compression)
    float build_dram_budget_gb;         // Memory budget in GB (0=unlimited)
    uint32_t num_threads;               // Number of threads for parallel build
    Metric metric_type;                 // Distance metric (L2, IP, COSINE)
    bool accelerate_build;              // Enable fast build (~30% faster, -1% recall)
    bool single_file_index;             // Save all in one file vs multiple files
    const char* tag_file;               // Optional: path to tag/id mapping file
    
    // Default constructor with sensible defaults
    DiskIndexBuildConfig()
        : max_degree(48),
          search_list_size(128),
          pq_dims(0),
          build_dram_budget_gb(0),
          num_threads(std::thread::hardware_concurrency()),
          metric_type(Metric::L2),
          accelerate_build(false),
          single_file_index(true),
          tag_file(nullptr) {
    }
    
    // Validation method
    bool validate(std::string& error_msg) const {
        if (data_path.empty()) {
            error_msg = "data_path cannot be empty";
            return false;
        }
        if (index_prefix.empty()) {
            error_msg = "index_prefix cannot be empty";
            return false;
        }
        if (max_degree < 1 || max_degree > 2048) {
            error_msg = "max_degree must be in range [1, 2048]";
            return false;
        }
        if (search_list_size < 1) {
            error_msg = "search_list_size must be >= 1";
            return false;
        }
        if (num_threads < 1) {
            error_msg = "num_threads must be >= 1";
            return false;
        }
        return true;
    }
};

// ============================================================================
// NEW: Detailed build result with metrics
// ============================================================================

struct BuildResult {
    bool success;                       // Whether build completed successfully
    std::string error_message;          // Error message if build failed
    int64_t num_points;                 // Number of points indexed
    uint32_t dimension;                 // Vector dimension
    double build_time_seconds;          // Total build time
    double index_size_mb;               // Final index size in MB
    std::string index_file_path;        // Path to the built index
    
    BuildResult() 
        : success(false), num_points(0), dimension(0), 
          build_time_seconds(0), index_size_mb(0) {
    }
    
    explicit BuildResult(bool s) : success(s), num_points(0), dimension(0), 
                                   build_time_seconds(0), index_size_mb(0) {
    }
};

// ============================================================================
// NEW: Forward declaration of existing legacy functions
// ============================================================================
// (Keep these for backward compatibility with existing code)

template<typename T, typename TagT = uint32_t>
bool build_disk_index(const char *dataFilePath, 
                      const char *indexFilePath, 
                      const char *indexBuildParameters,
                      pipeann::Metric _compareMetric, 
                      bool single_file_index, 
                      const char *tag_file = nullptr);

template<typename T, typename TagT = uint32_t>
bool build_disk_index_py(const char *dataPath, 
                         const char *indexFilePath, 
                         uint32_t R, 
                         uint32_t L, 
                         uint32_t M,
                         uint32_t num_threads, 
                         uint32_t PQ_bytes, 
                         pipeann::Metric _compareMetric,
                         bool single_file_index, 
                         const char *tag_file);

// ============================================================================
// NEW: Modern config-based API
// ============================================================================

template<typename T, typename TagT = uint32_t>
bool build_disk_index_from_config(const DiskIndexBuildConfig& config);

template<typename T, typename TagT = uint32_t>
BuildResult build_disk_index_with_result(const DiskIndexBuildConfig& config);

}  // namespace pipeann

// ============================================================================
// END OF aux_utils.h changes
// ============================================================================

/*
 * FILE: thirdparty/OdinANN/src/aux_utils.cpp
 * ==========================================
 * 
 * ADD these implementations at the end of the file:
 */

namespace pipeann {

// ============================================================================
// IMPLEMENTATION 1: Config-based wrapper (Phase 1)
// ============================================================================

template<typename T, typename TagT>
bool build_disk_index_from_config(const DiskIndexBuildConfig& config) {
    std::string error_msg;
    if (!config.validate(error_msg)) {
        std::cerr << "Config validation failed: " << error_msg << std::endl;
        return false;
    }
    
    // Map new config to legacy build_disk_index_py parameters
    // Note: M parameter (formerly 3rd uint32_t) mapped to 0 as it's not used
    return build_disk_index_py<T, TagT>(
        config.data_path.c_str(),
        config.index_prefix.c_str(),
        config.max_degree,              // R
        config.search_list_size,        // L
        0,                              // M (not used in modern pipeline)
        config.num_threads,
        config.pq_dims,                 // PQ_bytes
        config.metric_type,
        config.single_file_index,
        config.tag_file
    );
}

// ============================================================================
// IMPLEMENTATION 2: Result-based wrapper with metrics (Phase 2)
// ============================================================================

template<typename T, typename TagT>
BuildResult build_disk_index_with_result(const DiskIndexBuildConfig& config) {
    BuildResult result;
    
    // Validate input config
    if (!config.validate(result.error_message)) {
        result.success = false;
        return result;
    }
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    try {
        // Call the underlying build function
        bool build_success = build_disk_index_py<T, TagT>(
            config.data_path.c_str(),
            config.index_prefix.c_str(),
            config.max_degree,
            config.search_list_size,
            0,
            config.num_threads,
            config.pq_dims,
            config.metric_type,
            config.single_file_index,
            config.tag_file
        );
        
        if (!build_success) {
            result.success = false;
            result.error_message = "build_disk_index_py returned false";
            return result;
        }
        
        // Try to read metadata from built index
        // This is implementation-specific; adjust based on OdinANN's format
        // For now, we read from the data file to get num_points and dimension
        
        // TODO: Read index metadata file to populate:
        // - result.num_points
        // - result.dimension
        // - result.index_size_mb (sum of all generated files)
        
        // Placeholder: These should be read from actual index metadata
        result.num_points = 0;  // TODO: extract from index
        result.dimension = 0;   // TODO: extract from index
        
        // Calculate index files size
        std::string index_file = config.index_prefix;
        if (config.single_file_index) {
            index_file += ".odinann";
        } else {
            index_file += "_mem.index";
        }
        
        // Check if file exists and get size
        struct stat st;
        if (stat(index_file.c_str(), &st) == 0) {
            result.index_size_mb = (double)st.st_size / (1024 * 1024);
        }
        
        result.index_file_path = index_file;
        result.success = true;
        
    } catch (const std::exception& e) {
        result.success = false;
        result.error_message = std::string("Exception during build: ") + e.what();
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    result.build_time_seconds = 
        std::chrono::duration<double>(end_time - start_time).count();
    
    return result;
}

// ============================================================================
// EXPLICIT TEMPLATE INSTANTIATIONS
// ============================================================================
// Add these for commonly used types to avoid linker issues

template bool build_disk_index_from_config<float>(const DiskIndexBuildConfig& config);
template bool build_disk_index_from_config<float16>(const DiskIndexBuildConfig& config);

template BuildResult build_disk_index_with_result<float>(const DiskIndexBuildConfig& config);
template BuildResult build_disk_index_with_result<float16>(const DiskIndexBuildConfig& config);

}  // namespace pipeann

// ============================================================================
// END OF aux_utils.cpp changes
// ============================================================================

/*
 * FILE: thirdparty/OdinANN/CMakeLists.txt
 * ======================================
 * 
 * ENSURE these includes are available for compilation:
 * (Usually already present, just verify)
 * 
 * target_include_directories(odinann_lib PUBLIC
 *     ${CMAKE_CURRENT_SOURCE_DIR}/include
 * )
 * 
 * If building as external library:
 * 
 * set(CMAKE_CXX_STANDARD 17)  # Ensure C++17 or later
 */

/*
 * INTEGRATION SUMMARY FOR KNOWHERE DEVELOPERS
 * ===========================================
 * 
 * After applying these changes to thirdparty/OdinANN:
 * 
 * 1. Update Knowhere's odinann.cc Build() method to use:
 *    
 *    pipeann::DiskIndexBuildConfig config;
 *    config.data_path = build_conf.data_path.value();
 *    config.index_prefix = index_prefix_;
 *    config.max_degree = build_conf.max_degree.value();
 *    // ... set other fields ...
 *    
 *    auto result = pipeann::build_disk_index_with_result<DataType>(config);
 *    if (!result.success) {
 *        return Status::diskann_inner_error;
 *    }
 * 
 * 2. Update file registration to handle:
 *    - config.single_file_index = true: register {prefix}.odinann
 *    - config.single_file_index = false: register multiple files
 * 
 * 3. Test with both single-file and multi-file modes
 * 
 * 4. Ensure FileManager handles the generated index files for
 *    upload to Minio/external storage
 */
