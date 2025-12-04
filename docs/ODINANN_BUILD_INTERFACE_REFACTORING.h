// OdinANN Build Interface Refactoring Guide for Knowhere Integration
// 
// Current OdinANN API Analysis & Recommended Changes
// ===================================================

/*
 * CURRENT INTERFACE (from thirdparty/OdinANN/include/aux_utils.h)
 * ================================================================
 * 
 * template<typename T, typename TagT = uint32_t>
 * bool build_disk_index_py(const char *dataPath,
 *                          const char *indexFilePath,
 *                          uint32_t R,
 *                          uint32_t L,
 *                          uint32_t M,
 *                          uint32_t num_threads,
 *                          uint32_t PQ_bytes,
 *                          pipeann::Metric _compareMetric,
 *                          bool single_file_index,
 *                          const char *tag_file);
 * 
 * Issues with current interface:
 * ==============================
 * 1. Parameter naming: R, L, M are vague; should use meaningful names
 * 2. No structured config parameter; too many individual parameters
 * 3. No return detailed error codes/messages
 * 4. PQ_bytes parameter unclear (should be disk_pq_dims or pq_code_budget)
 * 5. No support for build callbacks/progress tracking
 */

/*
 * RECOMMENDED NEW INTERFACE FOR KNOWHERE
 * ======================================
 * 
 * File: thirdparty/OdinANN/include/aux_utils.h
 * 
 * Add a structured config class:
 */

namespace pipeann {

struct DiskIndexBuildConfig {
    std::string data_path;           // Input data file path
    std::string index_prefix;        // Output index file path prefix
    uint32_t max_degree;             // R: vamana graph degree (formerly R)
    uint32_t search_list_size;       // L: search list size during build (formerly L)
    uint32_t pq_dims;                // PQ compression dimensions (formerly PQ_bytes)
    float build_dram_budget_gb;      // Memory budget for building (new)
    uint32_t num_threads;            // Number of build threads
    Metric metric_type;              // Distance metric (L2, IP, COSINE)
    bool accelerate_build;           // Fast build mode (new)
    bool single_file_index;          // Save all in one file
    const char* tag_file;            // Optional tag file path
    
    // Default constructor
    DiskIndexBuildConfig()
        : max_degree(48),
          search_list_size(128),
          pq_dims(0),
          build_dram_budget_gb(0),
          num_threads(std::thread::hardware_concurrency()),
          metric_type(Metric::L2),
          accelerate_build(false),
          single_file_index(true),
          tag_file(nullptr) {}
};

struct BuildResult {
    bool success;
    std::string error_message;
    int64_t num_points;
    uint32_t dimension;
    double build_time_seconds;
    double index_size_mb;
    
    BuildResult() 
        : success(false), num_points(0), dimension(0), 
          build_time_seconds(0), index_size_mb(0) {}
};

}  // namespace pipeann

/*
 * NEW API - Option 1: Simple wrapper (recommended for Phase 1)
 * ===========================================================
 */

namespace pipeann {

template<typename T, typename TagT = uint32_t>
bool build_disk_index_from_config(const DiskIndexBuildConfig& config);

// Overload with detailed result reporting
template<typename T, typename TagT = uint32_t>
BuildResult build_disk_index_with_result(const DiskIndexBuildConfig& config);

}  // namespace pipeann

/*
 * IMPLEMENTATION NOTES FOR NEW API
 * ================================
 * 
 * File: thirdparty/OdinANN/src/aux_utils.cpp (to be modified)
 * 
 * 1. Keep existing build_disk_index_py for backward compatibility
 * 
 * 2. Add new wrapper function:
 * 
 *    template<typename T, typename TagT>
 *    bool build_disk_index_from_config(const DiskIndexBuildConfig& config) {
 *        // Extract parameters from config
 *        // Call existing build logic
 *        // Handle errors gracefully
 *    }
 * 
 * 3. Add detailed error reporting:
 * 
 *    template<typename T, typename TagT>
 *    BuildResult build_disk_index_with_result(const DiskIndexBuildConfig& config) {
 *        BuildResult result;
 *        auto start_time = std::chrono::high_resolution_clock::now();
 *        
 *        try {
 *            // Call build logic
 *            // Populate num_points, dimension from index metadata
 *            // Calculate index_size_mb
 *            result.success = true;
 *        } catch (const std::exception& e) {
 *            result.error_message = e.what();
 *            result.success = false;
 *        }
 *        
 *        auto end_time = std::chrono::high_resolution_clock::now();
 *        result.build_time_seconds = 
 *            std::chrono::duration<double>(end_time - start_time).count();
 *        
 *        return result;
 *    }
 * 
 * 4. Update Knowhere's odinann.cc to use:
 *    - Call build_disk_index_from_config or build_disk_index_with_result
 *    - Map pipeann::Metric to knowhere metric types
 *    - Handle file list generation for file manager registration
 */

/*
 * FILE NAMING CONVENTION FOR ODINANN INDEX
 * ========================================
 * 
 * Based on current OdinANN implementation:
 * 
 * If single_file_index = true:
 *   - {index_prefix}.odinann     (combined index file)
 * 
 * If single_file_index = false:
 *   - {index_prefix}_mem.index    (in-memory index structure)
 *   - {index_prefix}_disk.index   (disk index structure)
 *   - {index_prefix}_data.bin     (raw data vectors)
 *   - {index_prefix}_metadata     (metadata file)
 * 
 * With optional files:
 *   - {index_prefix}_cache.bin    (cached nodes)
 *   - {index_prefix}_tags        (tag mappings, if using tags)
 * 
 * Knowhere should list these files and register them with FileManager
 * for upload to Minio/external storage.
 */

/*
 * MIGRATION STEPS
 * ===============
 * 
 * Phase 1 (Minimal changes):
 * 1. Add DiskIndexBuildConfig struct to aux_utils.h
 * 2. Add build_disk_index_from_config wrapper
 * 3. Update Knowhere's odinann.cc to use new wrapper
 * 4. Test with single_file_index = true (simpler file management)
 * 
 * Phase 2 (Enhanced features):
 * 1. Add BuildResult struct with detailed reporting
 * 2. Implement build_disk_index_with_result
 * 3. Add progress callback support
 * 4. Support multi-file index mode
 * 
 * Phase 3 (Production hardening):
 * 1. Add comprehensive error handling
 * 2. Implement index validation
 * 3. Add recovery from incomplete builds
 * 4. Performance optimization and profiling
 */

/*
 * EXAMPLE USAGE IN KNOWHERE (odinann.cc)
 * ======================================
 * 
 * // In OdinANNIndexNode::Build()
 * 
 * pipeann::DiskIndexBuildConfig config;
 * config.data_path = build_conf.data_path.value();
 * config.index_prefix = index_prefix_;
 * config.max_degree = build_conf.max_degree.value();
 * config.search_list_size = build_conf.search_list_size.value();
 * config.pq_dims = build_conf.disk_pq_dims.value();
 * config.build_dram_budget_gb = build_conf.build_dram_budget_gb.value();
 * config.num_threads = build_conf.num_build_thread.value_or(
 *     std::thread::hardware_concurrency());
 * config.metric_type = metric;  // Already converted
 * config.accelerate_build = build_conf.accelerate_build.value();
 * config.single_file_index = true;
 * 
 * RETURN_IF_ERROR(TryOdinANNCall([&]() {
 *     auto result = pipeann::build_disk_index_with_result<DataType>(config);
 *     if (!result.success) {
 *         throw std::runtime_error("OdinANN build failed: " + result.error_message);
 *     }
 *     count_.store(result.num_points);
 *     dim_.store(result.dimension);
 * }));
 * 
 * // File registration with FileManager
 * std::vector<std::string> index_files;
 * if (config.single_file_index) {
 *     index_files.push_back(config.index_prefix + ".odinann");
 * } else {
 *     index_files.push_back(config.index_prefix + "_mem.index");
 *     index_files.push_back(config.index_prefix + "_disk.index");
 *     index_files.push_back(config.index_prefix + "_data.bin");
 * }
 * 
 * for (auto& filename : index_files) {
 *     if (!AddFile(filename)) {
 *         LOG_KNOWHERE_ERROR_ << "Failed to add file " << filename;
 *         return Status::disk_file_error;
 *     }
 * }
 */
