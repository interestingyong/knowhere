// Copyright (C) 2019-2023 Zilliz. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file except in compliance
// with the License. You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software distributed under the License
// is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express
// or implied. See the License for the specific language governing permissions and limitations under the License.

#include "knowhere/feder/OdinANN.h"

#include <atomic>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <mutex>
#include <condition_variable>
#include <sstream>
#include <thread>

#include "aux_utils.h"
#include "filemanager/FileManager.h"
#include "fmt/core.h"
#include "index/odinann/odinann_config.h"
#include "knowhere/comp/index_param.h"
#include "knowhere/dataset.h"
#include "knowhere/expected.h"
#include "knowhere/feature.h"
#include "knowhere/index/index_factory.h"
#include "knowhere/log.h"
#include "knowhere/prometheus_client.h"
#include "knowhere/thread_pool.h"
#include "knowhere/utils.h"

// OdinANN headers
#include "index.h"
#include "linux_aligned_file_reader.h"
#include "ssd_index.h"
#include "partition_and_pq.h"


namespace knowhere {

template <typename DataType>
class OdinANNIndexNode : public IndexNode {
    static_assert(KnowhereFloatTypeCheck<DataType>::value,
                  "OdinANN only support floating point data type(float32, float16, bfloat16)");

 public:
    using DistType = float;
    OdinANNIndexNode(const int32_t& version, const Object& object) : is_prepared_(false), dim_(-1), count_(-1) {
        assert(typeid(object) == typeid(Pack<std::shared_ptr<milvus::FileManager>>));
        auto odinann_index_pack = dynamic_cast<const Pack<std::shared_ptr<milvus::FileManager>>*>(&object);
        assert(odinann_index_pack != nullptr);
        file_manager_ = odinann_index_pack->GetPack();
        LOG_KNOWHERE_INFO_ << "version:" << version << " filemanager: " << file_manager_ <<  " OdinANNIndexNode created.";
    }

    Status
    Build(const DataSetPtr dataset, std::shared_ptr<Config> cfg, bool use_knowhere_build_pool) override;

    Status
    Train(const DataSetPtr dataset, std::shared_ptr<Config> cfg, bool use_knowhere_build_pool) override {
        return Status::not_implemented;
    }

    Status
    Add(const DataSetPtr dataset, std::shared_ptr<Config> cfg, bool use_knowhere_build_pool) override {
        return Status::not_implemented;
    }

    expected<DataSetPtr>
    Search(const DataSetPtr dataset, std::unique_ptr<Config> cfg, const BitsetView& bitset,
           milvus::OpContext* op_context) const override;

    expected<DataSetPtr>
    GetVectorByIds(const DataSetPtr dataset, milvus::OpContext* op_context) const override;

    expected<std::vector<IndexNode::IteratorPtr>>
    AnnIterator(const DataSetPtr dataset, std::unique_ptr<Config> cfg, const BitsetView& bitset,
                bool use_knowhere_search_pool, milvus::OpContext* op_context) const override;

    static bool
    StaticHasRawData(const knowhere::BaseConfig& config, const IndexVersion& version) {
        knowhere::MetricType metric_type = config.metric_type.has_value() ? config.metric_type.value() : "";
        return IsMetricType(metric_type, metric::L2) || IsMetricType(metric_type, metric::COSINE);
    }

    bool
    HasRawData(const std::string& metric_type) const override {
        return IsMetricType(metric_type, metric::L2) || IsMetricType(metric_type, metric::COSINE);
    }

    expected<DataSetPtr>
    GetIndexMeta(std::unique_ptr<Config> cfg) const override;

    Status
    Serialize(BinarySet& binset) const override {
        LOG_KNOWHERE_INFO_ << "OdinANN does nothing for serialize";
        return Status::success;
    }

    static expected<Resource>
    StaticEstimateLoadResource(const uint64_t file_size_in_bytes, const int64_t num_rows, const int64_t dim,
                               const knowhere::BaseConfig& config, const IndexVersion& version) {
        return Resource{file_size_in_bytes / 4, file_size_in_bytes};
    }

    Status
    Deserialize(const BinarySet& binset, std::shared_ptr<Config> cfg) override;

    Status
    DeserializeFromFile(const std::string& filename, std::shared_ptr<Config> config) override {
        LOG_KNOWHERE_ERROR_ << "OdinANN doesn't support Deserialization from file.";
        return Status::not_implemented;
    }

    static std::unique_ptr<BaseConfig>
    StaticCreateConfig() {
        return std::make_unique<OdinANNConfig>();
    }

    std::unique_ptr<BaseConfig>
    CreateConfig() const override {
        return StaticCreateConfig();
    }

    Status
    SetFileManager(std::shared_ptr<milvus::FileManager> file_manager) {
        if (file_manager == nullptr) {
            LOG_KNOWHERE_ERROR_ << "Malloc error, file_manager = nullptr.";
            return Status::malloc_error;
        }
        file_manager_ = file_manager;
        return Status::success;
    }

    int64_t
    Dim() const override {
        if (dim_.load() == -1) {
            LOG_KNOWHERE_ERROR_ << "Dim() function is not supported when index is not ready yet.";
            return 0;
        }
        return dim_.load();
    }

    int64_t
    Size() const override {
        // OdinANN index size calculation
        return 0;  // TODO: implement size calculation
    }

    int64_t
    Count() const override {
        if (count_.load() == -1) {
            LOG_KNOWHERE_ERROR_ << "Count() function is not supported when index is not ready yet.";
            return 0;
        }
        return count_.load();
    }

    std::string
    Type() const override {
        return knowhere::IndexEnum::INDEX_ODINANN;
    }

 private:
    bool
    LoadFile(const std::string& filename) {
        if (!file_manager_->LoadFile(filename)) {
            LOG_KNOWHERE_ERROR_ << "Failed to load file " << filename << ".";
            return false;
        }
        return true;
    }

    bool
    AddFile(const std::string& filename) {
        if (!file_manager_->AddFile(filename)) {
            LOG_KNOWHERE_ERROR_ << "Failed to add file " << filename << ".";
            return false;
        }
        return true;
    }

    std::string index_prefix_;
    mutable std::mutex preparation_lock_;
    std::atomic_bool is_prepared_;
    std::shared_ptr<milvus::FileManager> file_manager_;
    std::atomic_int64_t dim_;
    std::atomic_int64_t count_;
    std::shared_ptr<ThreadPool> search_pool_;
    // Underlying OdinANN SSDIndex instance for disk-resident index
    std::unique_ptr<pipeann::SSDIndex<DataType>> ssd_index_;
    // File reader for disk access
    std::shared_ptr<pipeann::AlignedFileReader> file_reader_;
};

}  // namespace knowhere

// Global memory index cache
// Stores the path to the shared memory index built from sampling
// All indices share one global mem_index to avoid duplicate construction
namespace knowhere {
namespace {
static std::mutex g_mem_index_lock;
static std::condition_variable g_mem_index_cv;
static std::string g_mem_index_path;  // Cached global mem_index path (shared by all indices)
static bool g_mem_index_building = false;  // Flag to indicate if mem_index is being built
// Single global shared mem_index (no per-type cache)
static std::shared_ptr<void> g_global_mem_index;  // holds std::shared_ptr<pipeann::Index<...>> casted to void
}  // namespace

static constexpr float kCacheExpansionRate = 1.2;

Status
TryOdinANNCall(std::function<void()>&& odinann_call) {
    try {
        odinann_call();
        return Status::success;
    } catch (const std::exception& e) {
        LOG_KNOWHERE_ERROR_ << "OdinANN Exception: " << e.what();
        return Status::odinann_inner_error;
    }
}

std::vector<std::string>
GetNecessaryFilenames(const std::string& prefix) {
    std::vector<std::string> filenames;
    // OdinANN build_disk_index generates:
    // - prefix_pq_pivots.bin
    // - prefix_pq_compressed.bin
    // - prefix_disk.index
    filenames.push_back(prefix + "_pq_pivots.bin");
    filenames.push_back(prefix + "_pq_compressed.bin");
    filenames.push_back(prefix + "_disk.index");
    filenames.push_back(prefix + "_disk.index_medoids.bin");
    filenames.push_back(prefix + "_disk.index_centroids.bin");
    return filenames;
}

std::vector<std::string>
GetOptionalFilenames(const std::string& prefix) {
    std::vector<std::string> filenames;
    // Optional files for OdinANN (warmup sample file)
    filenames.push_back(prefix + "_sample.bin");
    // mem index file generated when mem_L > 0 during build
    filenames.push_back(prefix + "_mem.index");
    return filenames;
}

inline bool
AnyIndexFileExist(const std::string& index_prefix) {
    auto file_exist = [](std::vector<std::string> filenames) -> bool {
        for (auto& filename : filenames) {
            if (pipeann::file_exists(filename)) {
                return true;
            }
        }
        return false;
    };
    return file_exist(GetNecessaryFilenames(index_prefix)) || file_exist(GetOptionalFilenames(index_prefix));
}

inline bool
CheckMetric(const std::string& metric) {
    if (metric != knowhere::metric::L2 && metric != knowhere::metric::IP && metric != knowhere::metric::COSINE) {
        LOG_KNOWHERE_ERROR_ << "OdinANN currently only supports floating point data for L2, IP, and COSINE metrics.";
        return false;
    }
    return true;
}


template <typename DataType>
Status
OdinANNIndexNode<DataType>::Build(const DataSetPtr dataset, std::shared_ptr<Config> cfg, bool use_knowhere_build_pool) {
    assert(file_manager_ != nullptr);
    std::lock_guard<std::mutex> lock(preparation_lock_);
    auto build_conf = static_cast<const OdinANNConfig&>(*cfg);

    if (!CheckMetric(build_conf.metric_type.value())) {
        LOG_KNOWHERE_ERROR_ << "Invalid metric type: " << build_conf.metric_type.value();
        return Status::invalid_metric_type;
    }

    if (!(build_conf.index_prefix.has_value() && build_conf.data_path.has_value())) {
        LOG_KNOWHERE_ERROR_ << "OdinANN file path for build is empty." << std::endl;
        return Status::invalid_param_in_json;
    }

    if (AnyIndexFileExist(build_conf.index_prefix.value())) {
        LOG_KNOWHERE_ERROR_ << "This index prefix already has index files." << std::endl;
        return Status::disk_file_error;
    }

    if (!LoadFile(build_conf.data_path.value())) {
        LOG_KNOWHERE_ERROR_ << "Failed to load the raw data before building." << std::endl;
        return Status::disk_file_error;
    }

    index_prefix_ = build_conf.index_prefix.value();

    // Get metadata from data file using get_bin_metadata
    size_t count = 0, dim = 0;
    RETURN_IF_ERROR(TryOdinANNCall([&]() { pipeann::get_bin_metadata(build_conf.data_path.value(), count, dim); }));
    count_.store(count);
    dim_.store(dim);

    // Convert metric type
    pipeann::Metric metric = pipeann::Metric::L2;
    if (IsMetricType(build_conf.metric_type.value(), knowhere::metric::IP)) {
        metric = pipeann::Metric::INNER_PRODUCT;
    } else if (IsMetricType(build_conf.metric_type.value(), knowhere::metric::COSINE)) {
        metric = pipeann::Metric::COSINE;
    }

    // Build index build parameters string: R L B M T [C]
    // R = max_degree, L = search_list_size, B = pq_code_budget_gb,
    // M = build_dram_budget_gb, T = num_threads, C (optional) = disk_pq_dims
    std::ostringstream param_stream;
    param_stream << build_conf.max_degree.value() << " " << build_conf.search_list_size.value() << " "
                 << build_conf.pq_code_budget_gb.value() << " " << build_conf.build_dram_budget_gb.value() << " "
                 << build_conf.num_build_thread.value_or(std::thread::hardware_concurrency()) << " "
                 << build_conf.disk_pq_dims.value();
    std::string index_build_parameters = param_stream.str();

    LOG_KNOWHERE_INFO_ << "datapath: " << build_conf.data_path.value() << " indexprefix: " << index_prefix_  << std::endl;                    

    // Call OdinANN build_disk_index
    RETURN_IF_ERROR(TryOdinANNCall([&]() {
        bool res = pipeann::build_disk_index<DataType>(build_conf.data_path.value().c_str(), index_prefix_.c_str(),
                                                       index_build_parameters.c_str(), metric,
                                                       true,    // single_file_index
                                                       nullptr  // tag_file
        );
        if (!res) {
            throw std::runtime_error("pipeann::build_disk_index failed");
        }
    }));

    // Add files to file manager
    for (auto& filename : GetNecessaryFilenames(index_prefix_)) {
        if (pipeann::file_exists(filename) && !AddFile(filename)) {
            LOG_KNOWHERE_ERROR_ << "Failed to add file " << filename << ".";
            return Status::disk_file_error;
        }
    }

    for (auto& filename : GetOptionalFilenames(index_prefix_)) {
        if (pipeann::file_exists(filename) && !AddFile(filename)) {
            LOG_KNOWHERE_ERROR_ << "Failed to add optional file " << filename << ".";
            // Don't return error for optional files
        }
    }

    // Build memory index if not exists (single global instance, one-time build for all indices)
    // Use a fixed global path so all indices share the same mem_index
    std::string mem_index_path;
    bool need_build = false;
    std::shared_ptr<pipeann::Index<DataType>> mem_index_ptr = nullptr;
    {
        std::unique_lock<std::mutex> lock(g_mem_index_lock);
        if (!g_mem_index_path.empty()) {
            mem_index_path = g_mem_index_path;
            LOG_KNOWHERE_INFO_ << "Using existing global memory index: " << mem_index_path;
        } else if (g_mem_index_building) {
            LOG_KNOWHERE_INFO_ << "Another thread is building mem_index, waiting...";
            // wait until the building flag is cleared (notified by builder)
            g_mem_index_cv.wait(lock, [] { return !g_mem_index_building; });
            if (!g_mem_index_path.empty()) {
                mem_index_path = g_mem_index_path;
                LOG_KNOWHERE_INFO_ << "Global memory index became available: " << mem_index_path;
            }
        } else {
            // This thread will build mem_index
            g_mem_index_building = true;
            need_build = true;
            mem_index_path = std::string(index_prefix_.c_str()) + "_odinann_global_mem.index";
        }
    }

    // If we need to build the mem_index and file doesn't exist, construct it
    if (need_build && !pipeann::file_exists(mem_index_path)) {
        double sampling_rate = build_conf.sampling_rate.has_value() ? static_cast<float>(build_conf.sampling_rate.value()) : 0.01f;
        double mem_index_alpha = build_conf.mem_index_alpha.has_value() ? static_cast<float>(build_conf.mem_index_alpha.value()) : 1.2f;

        LOG_KNOWHERE_INFO_ << "=== Building global memory index ===";
        LOG_KNOWHERE_INFO_ << "Target mem_index path: " << mem_index_path;
        LOG_KNOWHERE_INFO_ << "Sampling rate: " << sampling_rate << ", Alpha: " << mem_index_alpha;

        std::string sample_prefix = mem_index_path + "_sample";
        LOG_KNOWHERE_INFO_ << "Sample file prefix: " << sample_prefix;

        // Build mem_index inside TryOdinANNCall; capture mem_index_ptr
        Status build_mem_status = TryOdinANNCall([&]() {
            // gen_random_slice to create sample file from data
            LOG_KNOWHERE_INFO_ << "Generating random samples from data file...";
            pipeann::gen_random_slice<DataType>(build_conf.data_path.value(), sample_prefix, sampling_rate);
            LOG_KNOWHERE_INFO_ << "Sample generation completed.";

            // Build in-memory index from samples
            LOG_KNOWHERE_INFO_ << "Building in-memory index from samples...";
            pipeann::Parameters paras;
            paras.Set<unsigned>("R", 32);
            paras.Set<unsigned>("L", 64);
            paras.Set<unsigned>("C", 750);
            paras.Set<float>("alpha", mem_index_alpha);
            paras.Set<bool>("saturate_graph", 0);
            paras.Set<unsigned>("num_threads", build_conf.num_build_thread.value_or(std::thread::hardware_concurrency()));

            mem_index_ptr = std::make_shared<pipeann::Index<DataType>>(metric, static_cast<size_t>(dim_.load()),
                                                                       static_cast<size_t>(count_.load()), false, false, false);
            mem_index_ptr->build((sample_prefix + "_data.bin").c_str(), static_cast<size_t>(count_.load()), paras);
            LOG_KNOWHERE_INFO_ << "In-memory index built successfully.";

            LOG_KNOWHERE_INFO_ << "Saving in-memory index to disk...";
            mem_index_ptr->save(mem_index_path.c_str());
            LOG_KNOWHERE_INFO_ << "Global memory index saved to " << mem_index_path;
        });

        if (build_mem_status != Status::success) {
            LOG_KNOWHERE_ERROR_ << "Failed to build global memory index. Status: " << static_cast<int>(build_mem_status);
            LOG_KNOWHERE_INFO_ << "Cleaning up sample files...";
            // Try to remove sample files on build failure
            std::string sample_data_file = sample_prefix + "_data.bin";
            std::string sample_meta_file = sample_prefix + "_meta.bin";
            try {
                if (pipeann::file_exists(sample_data_file)) {
                    std::remove(sample_data_file.c_str());
                    LOG_KNOWHERE_INFO_ << "Removed sample file: " << sample_data_file;
                }
                if (pipeann::file_exists(sample_meta_file)) {
                    std::remove(sample_meta_file.c_str());
                    LOG_KNOWHERE_INFO_ << "Removed sample file: " << sample_meta_file;
                }
            } catch (const std::exception& e) {
                LOG_KNOWHERE_WARNING_ << "Exception while cleaning up sample files: " << e.what();
            }
            std::lock_guard<std::mutex> lock(g_mem_index_lock);
            g_mem_index_building = false;
            g_mem_index_cv.notify_all();
            return build_mem_status;
        }

        // Update global path and attach in-memory pointer under lock, then notify waiters
        {
            std::lock_guard<std::mutex> lock(g_mem_index_lock);
            g_mem_index_path = mem_index_path;
            if (mem_index_ptr) {
                g_global_mem_index = std::static_pointer_cast<void>(mem_index_ptr);
                LOG_KNOWHERE_INFO_ << "Global memory index in-memory pointer set and ready for use.";
            }
            g_mem_index_building = false;
            LOG_KNOWHERE_INFO_ << "=== Global memory index build complete ===";
            LOG_KNOWHERE_INFO_ << "Notifying " << "waiting threads...";
            g_mem_index_cv.notify_all();
        }
    } else {
        // If mem_index file already exists (or another thread built it), ensure global pointer is loaded
        std::lock_guard<std::mutex> lock(g_mem_index_lock);
        if (!mem_index_path.empty() && pipeann::file_exists(mem_index_path) && g_global_mem_index == nullptr) {
            LOG_KNOWHERE_INFO_ << "Mem_index file already exists, loading into global pointer: " << mem_index_path;
            try {
                auto mem_index_local = std::make_shared<pipeann::Index<DataType>>(metric, static_cast<size_t>(dim_.load()),
                                                                                   static_cast<size_t>(count_.load()), false, false, false);
                LOG_KNOWHERE_INFO_ << "Loading mem_index from file...";
                mem_index_local->load(mem_index_path.c_str());
                g_global_mem_index = std::static_pointer_cast<void>(mem_index_local);
                LOG_KNOWHERE_INFO_ << "Successfully loaded global memory index into g_global_mem_index.";
            } catch (const std::exception& e) {
                LOG_KNOWHERE_WARNING_ << "Failed to load existing mem_index into global pointer: " << e.what();
            }
        } else if (g_global_mem_index != nullptr) {
            LOG_KNOWHERE_INFO_ << "Global memory index already loaded in memory, skipping reload.";
        }
    }

    // Add memory index file to file manager if it exists
    if (!mem_index_path.empty() && pipeann::file_exists(mem_index_path)) {
        if (!AddFile(mem_index_path)) {
            LOG_KNOWHERE_WARNING_ << "Failed to add memory index file " << mem_index_path 
                                  << " to FileManager (not critical).";
        } else {
            LOG_KNOWHERE_INFO_ << "Memory index file added to FileManager.";
        }
    }

    is_prepared_.store(false);
    return Status::success;
}

template <typename DataType>
Status
OdinANNIndexNode<DataType>::Deserialize(const BinarySet& binset, std::shared_ptr<Config> cfg) {
    std::lock_guard<std::mutex> lock(preparation_lock_);
    auto prep_conf = static_cast<const OdinANNConfig&>(*cfg);

    if (!CheckMetric(prep_conf.metric_type.value())) {
        return Status::invalid_metric_type;
    }

    if (is_prepared_.load()) {
        return Status::success;
    }

    if (!prep_conf.index_prefix.has_value()) {
        LOG_KNOWHERE_ERROR_ << "OdinANN file path for deserialize is empty." << std::endl;
        return Status::invalid_param_in_json;
    }

    index_prefix_ = prep_conf.index_prefix.value();
    LOG_KNOWHERE_INFO_ << "indexprefix: " << index_prefix_ << std::endl;
    // Load files from file manager
    for (auto& filename : GetNecessaryFilenames(index_prefix_)) {
        if (!LoadFile(filename)) {
            LOG_KNOWHERE_ERROR_ << "Failed to load necessary file: " << filename;
            return Status::disk_file_error;
        }
    }

    for (auto& filename : GetOptionalFilenames(index_prefix_)) {
        auto is_exist_op = file_manager_->IsExisted(filename);
        if (!is_exist_op.has_value()) {
            LOG_KNOWHERE_ERROR_ << "Failed to check existence of file " << filename << ".";
            return Status::disk_file_error;
        }
        if (is_exist_op.value() && !LoadFile(filename)) {
            LOG_KNOWHERE_WARNING_ << "Failed to load optional file: " << filename;
            // Don't fail on optional files
        }
    }

    // Set thread pool
    search_pool_ = ThreadPool::GetGlobalSearchThreadPool();

    // Load OdinANN SSD index structure
    RETURN_IF_ERROR(TryOdinANNCall([&]() {
        // choose metric
        pipeann::Metric metric = pipeann::Metric::L2;
        if (IsMetricType(prep_conf.metric_type.value(), knowhere::metric::IP)) {
            metric = pipeann::Metric::INNER_PRODUCT;
        } else if (IsMetricType(prep_conf.metric_type.value(), knowhere::metric::COSINE)) {
            metric = pipeann::Metric::COSINE;
        }

        file_reader_ = std::make_shared<pipeann::LinuxAlignedFileReader>();
        ssd_index_ = std::make_unique<pipeann::SSDIndex<DataType>>(metric, file_reader_, true, false, nullptr);

        int load_result = ssd_index_->load(index_prefix_.c_str(), static_cast<uint32_t>(search_pool_->size()),
                                           true,   // new_index_format
                                           false   // use_page_search
        );
        if (load_result != 0) {
            throw std::runtime_error("Failed to load SSDIndex: " + std::to_string(load_result));
        }

        // update dim_ and count_ from loaded index
        uint64_t num_pts = ssd_index_->return_nd();
        count_.store(static_cast<int64_t>(num_pts));
        dim_.store(static_cast<int64_t>(ssd_index_->data_dim));

        // Load global memory index if available (single global instance, not per-instance)
        {
            std::lock_guard<std::mutex> lock(g_mem_index_lock);
            if (!g_mem_index_path.empty() && pipeann::file_exists(g_mem_index_path)) {
                LOG_KNOWHERE_INFO_ << "Loading global memory index from: " << g_mem_index_path;
                if (g_global_mem_index == nullptr) {
                    auto mem_index = std::make_shared<pipeann::Index<DataType>>(metric,
                        static_cast<size_t>(ssd_index_->data_dim),
                        static_cast<size_t>(ssd_index_->return_nd()),
                        false, false, false);
                    mem_index->load(g_mem_index_path.c_str());
                    // store into single global pointer (cast to void)
                    g_global_mem_index = std::static_pointer_cast<void>(mem_index);
                    LOG_KNOWHERE_INFO_ << "Loaded global memory index into g_global_mem_index.";
                } else {
                    LOG_KNOWHERE_INFO_ << "Global memory index already loaded.";
                }
            }
        }
    }));

    is_prepared_.store(true);
    LOG_KNOWHERE_INFO_ << "End of OdinANN loading.";
    return Status::success;
}

template <typename DataType>
expected<DataSetPtr>
OdinANNIndexNode<DataType>::Search(const DataSetPtr dataset, std::unique_ptr<Config> cfg, const BitsetView& bitset,
                                   milvus::OpContext* op_context) const {
    if (!is_prepared_.load()) {
        LOG_KNOWHERE_ERROR_ << "OdinANN index not loaded.";
        return expected<DataSetPtr>::Err(Status::empty_index, "OdinANN not loaded");
    }

    auto search_conf = static_cast<const OdinANNConfig&>(*cfg);
    if (!CheckMetric(search_conf.metric_type.value())) {
        return expected<DataSetPtr>::Err(Status::invalid_metric_type, "unsupported metric type");
    }

    if (ssd_index_ == nullptr) {
        LOG_KNOWHERE_ERROR_ << "OdinANN search backend not initialized.";
        return expected<DataSetPtr>::Err(Status::not_implemented, "OdinANN search backend not initialized");
    }

    auto k = static_cast<uint64_t>(search_conf.k.value());
    auto lsearch = static_cast<unsigned>(search_conf.search_list_size.value());
    auto beamwidth = static_cast<uint64_t>(search_conf.beamwidth.value_or(8));
    auto nq = dataset->GetRows();
    auto dim = dataset->GetDim();
    auto xq = static_cast<const DataType*>(dataset->GetTensor());
    LOG_KNOWHERE_INFO_ << "k,lsearch,beamwidth: " << k << " " << lsearch << " " << beamwidth << std::endl;
    if (nq <= 0) {
        return expected<DataSetPtr>::Err(Status::invalid_args, "nq must be >= 1");
    }

    // allocate output buffers
    auto p_id = std::make_unique<int64_t[]>(k * nq);
    auto p_dist = std::make_unique<DistType[]>(k * nq);

    std::vector<folly::Future<folly::Unit>> futures;
    futures.reserve(nq);
    
    // Determine mem_L: use config value if mem_index is ready, otherwise force to 0
    uint32_t mem_L;
    std::shared_ptr<pipeann::Index<DataType>> shared_mem_index;
    {
        std::lock_guard<std::mutex> lock(g_mem_index_lock);
        if (g_mem_index_building || g_mem_index_path.empty()) {
            // mem_index not yet ready (still building or never built)
            // Force mem_L to 0 to avoid reading uninitialized memory
            mem_L = 0;
            shared_mem_index = nullptr;
            if (search_conf.mem_L.has_value() && search_conf.mem_L.value() > 0) {
                LOG_KNOWHERE_WARNING_ << "mem_index not ready yet. Forcing mem_L from " 
                                      << search_conf.mem_L.value() << " to 0 for safety.";
            }
        } else {
            // mem_index is ready, use configured mem_L value, default to 40
            mem_L = search_conf.mem_L.has_value() ? static_cast<uint32_t>(search_conf.mem_L.value()) : 40u;
            // Get shared mem_index from single global instance
            if (g_global_mem_index != nullptr) {
                shared_mem_index = std::static_pointer_cast<pipeann::Index<DataType>>(g_global_mem_index);
            }
        }
    }
    for (int64_t row = 0; row < nq; ++row) {
        futures.emplace_back(
            search_pool_->push([this, index = row, k, beamwidth, lsearch, dim, xq, mem_L, shared_mem_index,
                               p_id_ptr = p_id.get(), p_dist_ptr = p_dist.get()]() {
                try {
                    std::unique_ptr<uint32_t[]> res_tags(new uint32_t[k]);
                    std::unique_ptr<float[]> res_dists(new float[k]);
                    
                    const DataType* query = xq + (index * dim);
                    
                    // Use pipe_search_with_outer_memindex if shared_mem_index is available, otherwise use beam_search
                    size_t returned;
                    if (shared_mem_index != nullptr) {
                        returned = ssd_index_->pipe_search_with_outer_memindex(query, k, mem_L, lsearch, res_tags.get(),
                                                                              res_dists.get(), beamwidth, 
                                                                              shared_mem_index.get(), nullptr);
                    } else {
                        returned = ssd_index_->beam_search(query, k, mem_L, lsearch, res_tags.get(),
                                                          res_dists.get(), beamwidth, nullptr);
                    }
                    
                    for (size_t i = 0; i < returned && i < k; ++i) {
                        p_id_ptr[index * k + i] = static_cast<int64_t>(res_tags[i]);
                        p_dist_ptr[index * k + i] = static_cast<DistType>(res_dists[i]);
                    }
                    // fill rest with -1 / INF
                    for (size_t i = returned; i < k; ++i) {
                        p_id_ptr[index * k + i] = -1;
                        p_dist_ptr[index * k + i] = std::numeric_limits<DistType>::infinity();
                    }
                } catch (const std::exception& e) {
                    LOG_KNOWHERE_ERROR_ << "OdinANN search failed: " << e.what();
                    throw;
                }
            }));
    }

    if (TryOdinANNCall([&]() { WaitAllSuccess(futures); }) != Status::success) {
        return expected<DataSetPtr>::Err(Status::odinann_inner_error, "some odinann search failed");
    }

    auto res = GenResultDataSet(nq, k, std::move(p_id), std::move(p_dist));
    return res;
}

template <typename DataType>
expected<DataSetPtr>
OdinANNIndexNode<DataType>::GetVectorByIds(const DataSetPtr dataset, milvus::OpContext* op_context) const {
    if (!is_prepared_.load()) {
        LOG_KNOWHERE_ERROR_ << "OdinANN index not loaded.";
        return expected<DataSetPtr>::Err(Status::empty_index, "index not loaded");
    }

    if (ssd_index_ == nullptr) {
        LOG_KNOWHERE_ERROR_ << "OdinANN backend not initialized.";
        return expected<DataSetPtr>::Err(Status::not_implemented, "OdinANN backend not initialized");
    }

    // Note: SSDIndex doesn't support get_vector_by_tag directly.
    // For now, return not_implemented as this is a limitation of the disk index.
    LOG_KNOWHERE_WARNING_ << "OdinANN SSDIndex does not support GetVectorByIds operation.";
    return expected<DataSetPtr>::Err(Status::not_implemented,
                                     "OdinANN disk index does not support GetVectorByIds");
}

template <typename DataType>
expected<std::vector<IndexNode::IteratorPtr>>
OdinANNIndexNode<DataType>::AnnIterator(const DataSetPtr dataset, std::unique_ptr<Config> cfg,
                                        const BitsetView& bitset, bool use_knowhere_search_pool,
                                        milvus::OpContext* op_context) const {
    LOG_KNOWHERE_INFO_ << "OdinANN AnnIterator is not supported yet.";
    return expected<std::vector<IndexNode::IteratorPtr>>::Err(Status::not_implemented,
                                                              "OdinANN does not support AnnIterator");
}

template <typename DataType>
expected<DataSetPtr>
OdinANNIndexNode<DataType>::GetIndexMeta(std::unique_ptr<Config> cfg) const {
    auto odinann_conf = static_cast<const OdinANNConfig&>(*cfg);
    feder::odinann::OdinANNMeta meta(
        odinann_conf.data_path.value_or(""), odinann_conf.max_degree.value_or(48),
        odinann_conf.search_list_size.value_or(128), odinann_conf.pq_code_budget_gb.value_or(0),
        odinann_conf.build_dram_budget_gb.value_or(0), odinann_conf.disk_pq_dims.value_or(0),
        odinann_conf.accelerate_build.value_or(false), odinann_conf.sampling_rate.value_or(0.01f), 
        odinann_conf.mem_index_alpha.value_or(1.2f), Count(), std::vector<int64_t>());
    LOG_KNOWHERE_INFO_ << " data_path:" << odinann_conf.data_path.value() << std::endl;
    Json json_meta;
    nlohmann::to_json(json_meta, meta);
    return GenResultDataSet(json_meta.dump(), "");
}

#ifdef KNOWHERE_WITH_CARDINAL
KNOWHERE_SIMPLE_REGISTER_DENSE_FLOAT_ALL_GLOBAL(ODINANN_DEPRECATED, OdinANNIndexNode, knowhere::feature::DISK)
#else
KNOWHERE_SIMPLE_REGISTER_DENSE_FLOAT_ALL_GLOBAL(ODINANN, OdinANNIndexNode, knowhere::feature::DISK)
#endif

}  // namespace knowhere
