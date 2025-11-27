https://infiniflow.org/docs/dev/benchmark
https://github.com/infiniflow/infinity/blob/main/python/benchmark/README.md
git clone https://github.com/interestingyong/infinity.git
cd infinity/python/benchmark/
python3.10 -m venv venv
source venv/bin/activate
sudo apt-get update
sudo apt-get install -y pkg-config libhdf5-dev python3-dev
pip install numpy -i https://pypi.tuna.tsinghua.edu.cn/simple
pip install --no-cache-dir --no-binary=h5py h5py -i https://pypi.tuna.tsinghua.edu.cn/simple
pip install -r requirements.txt -i https://pypi.tuna.tsinghua.edu.cn/simple

 aria2c -x 8 http://ann-benchmarks.com/sift-128-euclidean.hdf5
aria2c -x 8 http://ann-benchmarks.com/gist-960-euclidean.hdf5
aria2c -x 8 https://home.apache.org/~mikemccand/enwiki-20120502-lines-1k.txt.lzma  失效，通过lucenutil仓库脚本下载，放到amazons3了。
export http_proxy=http://192.168.1.202:8889;git clone https://github.com/mikemccand/luceneutil.git
 python src/python/initial_setup.py -download  
create data directory at /home/ictrek/workspace-docker/wy/dataset/data
create indices directory at /home/ictrek/workspace-docker/wy/dataset/indices
download https://luceneutil-corpus-files.s3.ca-central-1.amazonaws.com/enwiki-20120502-lines-1k-fixed-utf8-with-random-label.txt.lzma to /home/ictrek/workspace-docker/wy/dataset/data/enwiki-20120502-lines-1k-fixed-utf8-with-random-label.txt.lzma - might take a long time!
 downloading ... 1%, 82.45 MB/6007.44MB, speed 3593.79 KB/ss

aria2c -x 8 ftp://ftp.irisa.fr/local/texmex/corpus/sift.tar.gz -c
aria2c -x 8 ftp://ftp.irisa.fr/local/texmex/corpus/gist.tar.gz -c




export PRE=/home/ictrek/workspace-docker/wy
mkdir -p $PRE/elasticsearch/data
chown -R 1000:0 elasticsearch/data
 chmod -R 770 elasticsearch/data
docker run -d --name elasticsearch --network host -e "discovery.type=single-node" -e "ES_JAVA_OPTS=-Xms16384m -Xmx32000m" -e "xpack.security.enabled=false" -v $PRE/elasticsearch/data:/usr/share/elasticsearch/data elasticsearch:8.13.4
 pip install requests -i https://pypi.tuna.tsinghua.edu.cn/simple

mkdir -p $PRE/qdrant/storage
docker run -d --name qdrant --network host -v $PRE/qdrant/storage:/qdrant/storage qdrant/qdrant:v1.9.2
mkdir -p $PRE/quickwit
docker run -d --name quickwit --network=host -v $PRE/quickwit/qwdata:/quickwit/qwdata quickwit/quickwit:0.8.1 run
mkdir -p $PRE/infinity
docker run -d --name infinity --network=host -v $PRE/infinity:/var/infinity --ulimit nofile=500000:500000 infiniflow/infinity:nightly


 pip  install ./python/infinity_sdk/ -i https://pypi.tuna.tsinghua.edu.cn/simple
cd  /home/ictrek/workspace-docker/wy/infinity/python/benchmark
 cp ../../../dataset/*.hdf5 datasets/
 /home/ictrek/workspace-docker/wy/infinity/python/benchmark/datasets/sift/sift-128-euclidean.hdf5


修改infinity的测试代码，解析es返回值有问题。
 /home/ictrek/workspace-docker/wy/infinity/python/benchmark/clients/base_client.py
 for i, result in enumerate(results):
392                         ids = []
393                         for item in result[1:]:
394                             doc_id = item[0] if isinstance(item, tuple) else item
395                             if isinstance(doc_id, str):
396                                 try:
397                                     doc_id = int(doc_id)
398                                 except ValueError:
399                                     continue  # 跳过无效ID
400                             processed_id = ((doc_id >> 32) << 23) + (doc_id & 0xFFFFFFFF)
401                     x        ids.append(processed_id)
402                         precision = (
403                             len(set(ids).intersection(expected_result[i][1:]))
404                             / self.data["topK"]
405                         )
406                         precisions.append(precision)


 python run.py --dataset sift --engine elasticsearch --import
python run.py --query=16 --engine elasticsearch --dataset sift   查mean_time（avglat） ,mean_precisions（recall）
python run.py --dataset sift --engine elasticsearch --query-express=16

mivlus部署:
vectordbbench默认数据集的地方
/tmp/vectordb_bench/dataset/cohere/cohere_medium_1m 

Diskann arm
 ./build/apps/utils/fvecs_to_bin  float ../dataset/sift/sift_learn.fvecs ../dataset/sift/sift_learn.fbin
./build/apps/utils/fvecs_to_bin  float ../dataset/sift/sift_query.fvecs ../dataset/sift/sift_query.fbin
 ./build/apps/utils/compute_groundtruth  --data_type float --dist_fn l2 --base_file ../dataset/sift/sift_learn.fbin --query_file  ../dataset/sift/sift_query.fbin --gt_file ../dataset/sift/sift_query_learn_gt100 --K 100

--sift-1m-128dim
 ./build/apps/build_disk_index --data_type float --dist_fn l2 --data_path ../dataset/sift/sift_learn.fbin --index_path_prefix ../dataset/sift/disk_index_sift_learn_R32_L50_A1.2 -R 64 -L150 -B 0.15 -M 4
 --R 控制 Vamana 图中节点的最大连接数
--L   构建阶段的候选列表大小（值越大，图质量越高，但构建时间延长）范围：[1, int32_max]，默认值：100
--B 控制搜索时的内存分配    DRAM budget in GB for searching the index
                                      to set the compressed level for data
                                      while search happens
--M  DRAM budget in GB for building the index
mkdir ../dataset/sift/res
 ./build/apps/search_disk_index  --data_type float --dist_fn l2 --index_path_prefix ../dataset/sift/disk_index_sift_learn_R32_L50_A1.2 --query_file ../dataset/sift/sift_query.fbin  --gt_file ../dataset/sift/sift_query_learn_gt100 -K 10 -L 150 --result_path ../dataset/sift/res --num_nodes_to_cache 10000
--cohere-1m-768dim
 ./build/apps/build_disk_index --data_type float --dist_fn l2 --data_path ../dataset/cohere/cohere_learn.fbin --index_path_prefix ../dataset/cohere/disk_index_sift_learn_R64_L150_A1.2 -R 64 -L150 -B 0.15 -M 4
 ./build/apps/search_disk_index  --data_type float --dist_fn l2 --index_path_prefix ../dataset/cohere/disk_index_sift_learn_R32_L50_A1.2 --query_file ../dataset/cohere/cohere_query.fbin  --gt_file ../dataset/cohere/cohere_query_learn_gt100 -K 10 -L 150 --result_path ../dataset/cohere/res --num_nodes_to_cache 10000
 ./build/apps/search_disk_index  --data_type float --dist_fn l2 --index_path_prefix ../dataset/cohere/disk_index_sift_learn_R32_L50_A1.2 --query_file ../dataset/cohere/cohere_query.fbin  --gt_file ../dataset/cohere/cohere_query_learn_gt100 -K 10 -L 150 --result_path ../dataset/cohere/res --num_nodes_to_cache 10000 -T 16 (默认是8线程)

pipeann arm
- 利用diskann的文件,uint8/float都失败了
 export INDEX_PREFIX=/home/ictrek/workspace-docker/wy/dataset/cohere/disk_index_pipe_learn_R64_L150_A1.2
 export DATA_PATH=/home/ictrek/workspace-docker/wy/dataset/cohere/cohere_learn.fbin
 ./build/apps/build_disk_index --data_type float --dist_fn l2 --data_path ../dataset/cohere/cohere_learn.fbin --index_path_prefix ../dataset/cohere/disk_index_pipe_learn_R64_L150_A1.2 -R 64 -L150 -B 0.15 -M 4
 ./build/tests/utils/gen_random_slice float ${DATA_PATH} ${INDEX_PREFIX}_SAMPLE_RATE_0.01 0.01
./build/tests/build_memory_index float ${INDEX_PREFIX}_SAMPLE_RATE_0.01_data.bin ${INDEX_PREFIX}_SAMPLE_RATE_0.01_ids.bin ${INDEX_PREFIX}_mem.index 0 1 64 150 1.2 24 l2
./build/tests/search_disk_index float ${INDEX_PREFIX} 1 32 /home/ictrek/workspace-docker/wy/dataset/cohere/cohere_query.fbin /home/ictrek/workspace-docker/wy/dataset/cohere/cohere_query_learn_gt100 10 l2 2 10 10 20 30 40
./build/tests/search_disk_index float ${INDEX_PREFIX} 1 32 /home/ictrek/workspace-docker/wy/dataset/cohere/cohere_query.fbin /home/ictrek/workspace-docker/wy/dataset/cohere/cohere_query_learn_gt100 10 l2 2 10 150
[图片]
- 重新生成的文件
 ./build/apps/build_disk_index --data_type uint8 --dist_fn l2 --data_path ../dataset/cohere/cohere_learn.fbin --index_path_prefix ../dataset/cohere/disk_index_pipe_learn_R64_L150_A1.2 -R 64 -L150 -B 0.15 -M 4
 export INDEX_PREFIX=/home/ictrek/workspace-docker/wy/dataset/cohere/disk_index_pipe_learn_R64_L150_A1.2
 export DATA_PATH=/home/ictrek/workspace-docker/wy/dataset/cohere/cohere_learn.fbin
 ./build/tests/utils/gen_random_slice uint8 ${DATA_PATH} ${INDEX_PREFIX}_SAMPLE_RATE_0.01 0.01
./build/tests/build_memory_index uint8  ${INDEX_PREFIX}_SAMPLE_RATE_0.01_data.bin ${INDEX_PREFIX}_SAMPLE_RATE_0.01_ids.bin ${INDEX_PREFIX}_mem.index 0 1 64 150 1.2 24 l2
./build/tests/search_disk_index uint8 ${INDEX_PREFIX} 1 32 /home/ictrek/workspace-docker/wy/dataset/cohere/cohere_query.fbin /home/ictrek/workspace-docker/wy/dataset/cohere/cohere_query_learn_gt100 10 l2 2 10 150
[图片]
- 测试sift的数据集
 ./build/apps/build_disk_index --data_type uint8 --dist_fn l2 --data_path ../dataset/sift/sift_learn.fbin --index_path_prefix ../dataset/sift/disk_index_sift_learn_R64_L150_A1.2 -R 64 -L150 -B 0.15 -M 4 （DiskANN/build_disk_index）
 export INDEX_PREFIX=/home/ictrek/workspace-docker/wy/dataset/sift/disk_index_sift_learn_R64_L150_A1.2
 export DATA_PATH=/home/ictrek/workspace-docker/wy/dataset/sift/sift_learn.fbin
 ./build/tests/utils/gen_random_slice uint8 ${DATA_PATH} ${INDEX_PREFIX}_SAMPLE_RATE_0.01 0.01
./build/tests/build_memory_index uint8  ${INDEX_PREFIX}_SAMPLE_RATE_0.01_data.bin ${INDEX_PREFIX}_SAMPLE_RATE_0.01_ids.bin ${INDEX_PREFIX}_mem.index 0 1 64 150 1.2 24 l2
./build/tests/search_disk_index uint8 ${INDEX_PREFIX} 1 32 /home/ictrek/workspace-docker/wy/dataset/sift/sift_query.fbin /home/ictrek/workspace-docker/wy/dataset/sift/sift_query_learn_gt100 10 l2 2 10 150
[图片]
- 重新初始化测试
Usage:
（PipeANN/build_disk_index）
build/tests/build_disk_index uint8  ../dataset/sift/sift_learn.fbin  ../dataset/sift/sift_learn.fbin 96 128 3.3 256 112 l2 0  
export INDEX_PREFIX=/home/ictrek/workspace-docker/wy/dataset/sift/sift_learn.fbin
build/tests/utils/gen_random_slice uint8  /home/ictrek/workspace-docker/wy/dataset/sift/sift_learn.fbin ${INDEX_PREFIX}_SAMPLE_RATE_0.01 0.01
build/tests/build_memory_index uint8 ${INDEX_PREFIX}_SAMPLE_RATE_0.01_data.bin ${INDEX_PREFIX}_SAMPLE_RATE_0.01_ids.bin ${INDEX_PREFIX}_mem.index 0 0 32 64 1.2 24 l2
build/tests/utils/compute_groundtruth uint8 ../dataset/sift/sift_learn.fbin  / ../dataset/sift/sift_query.fbin  1000 ../dataset/sift/sift_query_learn_gt100

./build/tests/search_disk_index uint8 ${INDEX_PREFIX} 1 32 /home/ictrek/workspace-docker/wy/dataset/sift/sift_query.fbin /home/ictrek/workspace-docker/wy/dataset/sift/sift_query_learn_gt100 100 l2 2 10 150  (topk=100)


build/tests/build_disk_index uint8  ../dataset/cohere/cohere_learn.fbin  ../dataset/cohere/cohere_learn.fbin 96 128 3.3 256 112 l2 0  （PipeANN/build_disk_index）
export INDEX_PREFIX=/home/ictrek/workspace-docker/wy/dataset/cohere/cohere_learn.fbin
build/tests/utils/gen_random_slice uint8  /home/ictrek/workspace-docker/wy/dataset/cohere/cohere_learn.fbin ${INDEX_PREFIX}_SAMPLE_RATE_0.01 0.01
build/tests/build_memory_index uint8 ${INDEX_PREFIX}_SAMPLE_RATE_0.01_data.bin ${INDEX_PREFIX}_SAMPLE_RATE_0.01_ids.bin ${INDEX_PREFIX}_mem.index 0 0 32 64 1.2 24 l2
./build/tests/search_disk_index uint8 ${INDEX_PREFIX} 1 32 /home/ictrek/workspace-docker/wy/dataset/cohere/cohere_query.fbin /home/ictrek/workspace-docker/wy/dataset/cohere/cohere_query_learn_gt100 10 l2 2 10 150

[图片]
pipeann之前测试的方法
 export PATH=$PATH:build/tests/:build/:/home/ictrek/workspace-docker/wy/PipeANN/build/:/home/ictrek/workspace-docker/wy/PipeANN/build/tests/:/home/ictrek/workspace-docker/wy/PipeANN/build/tests/utils/
change_pts uint8 bigann.bin 1000000
mv bigann.bin1000000  1M.bin
 compute_groundtruth uint8 1M.bin bigann_query.bbin 1000 1M_gt.bin   (nearest neigbour =1000)
#Usage: build_disk_index <data_type (float/int8/uint8)>  <data_file.bin> <index_prefix_path> <R>  <L>  <B>  <M>  <T> <similarity metric (cosine/l2) case sensitive>. <single_file_index (0/1)> See README for more information on parameters.
build_disk_index uint8 1M.bin 1m 96 128 3.3 128 112 l2 0  (128: L  the size of search list we maintain during index building)
export INDEX_PREFIX=1m # 
export DATA_PATH=1M.bin
gen_random_slice uint8 ${DATA_PATH} ${INDEX_PREFIX}_SAMPLE_RATE_0.01 0.01
build_memory_index uint8 ${INDEX_PREFIX}_SAMPLE_RATE_0.01_data.bin ${INDEX_PREFIX}_SAMPLE_RATE_0.01_ids.bin ${INDEX_PREFIX}_mem.index 0 0 32 64 1.2 24 l2
#build/tests/search_disk_index <data_type> <index_prefix> <nthreads> <I/O pipeline width (max for PipeANN)> <query file> <truth file> <top-K> <similarity> <search_mode (2 for PipeANN)> <L of in-memory index> <Ls for on-disk index>
search_disk_index uint8 ${INDEX_PREFIX} 1 32 bigann_query.bbin 1M_gt.bin 10 l2 2 10 10 20 30 40
[图片]

使用bigann提取1M的数据测试
change_pts uint8 bigann.bin 1000000
mv bigann.bin1000000  1M.bin
 compute_groundtruth uint8 1M.bin bigann_query.bbin 1000 1M_gt.bin
#Usage: build_disk_index <data_type (float/int8/uint8)>  <data_file.bin> <index_prefix_path> <R>  <L>  <B>  <M>  <T> <similarity metric (cosine/l2) case sensitive>. <single_file_index (0/1)> See README for more information on parameters.
build_disk_index uint8 1M.bin 1m 96 128 3.3 128 112 l2 0  (128: L  the size of search list we maintain during index building)
export INDEX_PREFIX=1m # on-disk index file name prefix.
export DATA_PATH=1M.bin
gen_random_slice uint8 ${DATA_PATH} ${INDEX_PREFIX}_SAMPLE_RATE_0.01 0.01
build_memory_index uint8 ${INDEX_PREFIX}_SAMPLE_RATE_0.01_data.bin ${INDEX_PREFIX}_SAMPLE_RATE_0.01_ids.bin ${INDEX_PREFIX}_mem.index 0 0 32 64 1.2 24 l2
#build/tests/search_disk_index <data_type> <index_prefix> <nthreads> <I/O pipeline width (max for PipeANN)> <query file> <truth file> <top-K> <similarity> <search_mode (2 for PipeANN)> <L of in-memory index> <Ls for on-disk index>
search_disk_index uint8 ${INDEX_PREFIX} 1 32 bigann_query.bbin 1M_gt.bin 10 l2 2 10 10 20 30 40    需要注意10是topk，后面的10，20，30，40代表内存搜索节点的个数，也就是深度meml。 10，20，30，40是4个测试。 测试代码会跳过小于topk的meml测试。

使用bigann提取100M的数据测试
change_pts uint8 bigann.bin 100000000
mv bigann.bin100000000  100M.bin
 compute_groundtruth uint8 100M.bin bigann_query.bbin 1000 100M_gt.bin (内存占用很大，注意是否真的完成)
[图片]
#Usage: build_disk_index <data_type (float/int8/uint8)>  <data_file.bin> <index_prefix_path> <R>  <L>  <B>  <M>  <T> <similarity metric (cosine/l2) case sensitive>. <single_file_index (0/1)> See README for more information on parameters.
https://github.dev/interestingyong/PipeANN/blob/main/tests/search_disk_index.cpp#L673  
  " B (RAM limit of final index in GB) "  影响PQ对向量的编码的chunk数量， chunk少了RAM就少了。 calculate_num_pq_chunks
            " M (memory limit while indexing in GB)"    build_merged_vamana_index计算内存，如果内存足够，还是一个shot， 否则通过partition_with_ram_budget计算总共需要的内存/限制的内存，得到应该切成几个区域来index。避免内存过载。最早还是会通过merge_shards合并。
             " T (number of threads for indexing) "
build_disk_index uint8 100M.bin 100m 96 128 3.3 128 112 l2 0  (128: L  the size of search list we maintain during index building) e1001无法完成，内存过大。
build_disk_index uint8 100M.bin 100m 96 64 3.3 128 112 l2 0 e1001无法完成，内存过大。
build_disk_index uint8 100M.bin 100m 96 64 3.3 128 112 l2 1
 build_disk_index uint8 100M.bin 100m 96 64 3.3 32 16 l2 0  
export INDEX_PREFIX=100m 
export DATA_PATH=100M.bin
gen_random_slice uint8 ${DATA_PATH} ${INDEX_PREFIX}_SAMPLE_RATE_0.01 0.01
build_memory_index uint8 ${INDEX_PREFIX}_SAMPLE_RATE_0.01_data.bin ${INDEX_PREFIX}_SAMPLE_RATE_0.01_ids.bin ${INDEX_PREFIX}_mem.index 0 0 32 64 1.2 24 l2
#build/tests/search_disk_index <data_type> <index_prefix> <nthreads> <I/O pipeline width (max for PipeANN)> <query file> <truth file> <top-K> <similarity> <search_mode (2 for PipeANN)> <L of in-memory index> <Ls for on-disk index>
search_disk_index uint8 ${INDEX_PREFIX} 1 32 bigann_query.bbin 100M_gt.bin 10 l2 2 10 10 20 30 40    需要注意10是topk，后面的10，20，30，40代表内存搜索节点的个数，也就是深度meml。 10，20，30，40是4个测试。 测试代码会跳过小于topk的meml测试。


search-insert
将第二个 100M 向量插入到使用数据集中前 100M 向量构建的索引中，并发搜索。
../../PipeANN/build/tests/utils/compute_groundtruth uint8 bigann.bin bigann_query.bbin 1000 truth.bin
build/tests/gt_update truth.bin 200000000 1000000 10 1B_topk 1
每插入/删除 1M 个载体后计算召回率。


change_pts uint8 bigann.bin 2000000
mv bigann.bin2000000  2M.bin
../../PipeANN/build/tests/utils/compute_groundtruth uint8 2M.bin bigann_query.bbin 1000 2M_truth.bin

 ../../../PipeANN/build/tests/gt_update -h
Correct usage: ../../../PipeANN/build/tests/gt_update <file> <tot_npts> <batch_npts> <target_topk> <target_dir> <insert_only>
../../PipeANN/build/tests/gt_update 2M_truth.bin 2000000 10000 10 2M_topk 1
../../PipeANN/build/tests/test_insert_search uint8 2M.bin 64 10000 1 10 32 0 1m  bigann_query.bbin 2M_topk 0 10 4 4 0 20



search-insert-delete
插入第二个 100M 向量并删除数据集中的前 100M 向量，并发搜索。
每插入/删除 1M 个载体后计算召回率。


change_pts uint8 bigann.bin 2000000
mv bigann.bin2000000  2M.bin
../../PipeANN/build/tests/utils/compute_groundtruth uint8 2M.bin bigann_query.bbin 1000 2M_truth.bin
../../PipeANN/build/tests/gt_update 2M_truth.bin  2000000 10000 10 2M_topk 1 (这里不能些insert_only=0，有bug，解析完了一条数据都没有)
../../PipeANN/build/tests/overall_performance uint8 2M.bin 128 1m bigann_query.bbin 2M_topk 10 4 100 20 30 




pip install numpy pandas faiss-cpu pyarrow -i https://pypi.tuna.tsinghua.edu.cn/simple

 python ./conver_npy.py
 python convert_to_vdb_format.py   --train train_vectors.npy   --test test_vectors.npy   --out custom_dataset   --topk 100
 scp custom_dataset/ root@10.100.10.221://tmp/vectordb_bench/dataset/














格式转换脚本-hdf5到parquet
annbenchmark到vectordbbench的格式
cat conver_npy.py
import h5py
import numpy as np

# 下载并加载 HDF5 文件
with h5py.File('sift-128-euclidean.hdf5', 'r') as f:
    train_vectors = f['train'][:]  # 训练向量，形状 (num_samples, 128)
    test_vectors = f['test'][:]    # 测试向量
    neighbors = f['neighbors'][:] # 真实近邻 ID 列表

np.save("train_vectors.npy", train_vectors)
np.save("test_vectors.npy", test_vectors)





cat convert_to_vdb_format.py
import os
import argparse
import numpy as np
import pandas as pd
import faiss
from ast import literal_eval
from typing import Optional
def load_csv(path: str):
    df = pd.read_csv(path)
    if 'emb' not in df.columns:
        raise ValueError(f"CSV 文件中缺少 'emb' 列：{path}")
    df['emb'] = df['emb'].apply(literal_eval)
    if 'id' not in df.columns:
        df.insert(0, 'id', range(len(df)))
    return df
def load_npy(path: str):
    arr = np.load(path)
    df = pd.DataFrame({
        'id': range(arr.shape[0]),
        'emb': arr.tolist()
    })
    return df
def load_vectors(path: str) -> pd.DataFrame:
    if path.endswith('.csv'):
        return load_csv(path)
    elif path.endswith('.npy'):
        return load_npy(path)
    else:
        raise ValueError(f"不支持的文件格式: {path}")
def compute_ground_truth(train_vectors: np.ndarray, test_vectors: np.ndarray, top_k: int = 10):
    dim = train_vectors.shape[1]
    index = faiss.IndexFlatL2(dim)
    index.add(train_vectors)
    _, indices = index.search(test_vectors, top_k)
    return indices
def save_ground_truth(df_path: str, indices: np.ndarray):
    df = pd.DataFrame({
        "id": np.arange(indices.shape[0]),
        "neighbors_id": indices.tolist()
    })
    df.to_parquet(df_path, index=False)
    print(f"✅ Ground truth 保存成功: {df_path}")
def main(train_path: str, test_path: str, output_dir: str,
         label_path: Optional[str] = None, top_k: int = 10):
    os.makedirs(output_dir, exist_ok=True)
    # 加载训练和查询数据
    print("📥 加载训练数据...")
    train_df = load_vectors(train_path)
    print("📥 加载查询数据...")
    test_df = load_vectors(test_path)
    # 向量提取并转换为 numpy
    train_vectors = np.array(train_df['emb'].to_list(), dtype='float32')
    test_vectors = np.array(test_df['emb'].to_list(), dtype='float32')
    # 保存保留所有字段的 parquet 文件
    train_df.to_parquet(os.path.join(output_dir, 'train.parquet'), index=False)
    print(f"✅ train.parquet 保存成功，共 {len(train_df)} 条记录")
    test_df.to_parquet(os.path.join(output_dir, 'test.parquet'), index=False)
    print(f"✅ test.parquet 保存成功，共 {len(test_df)} 条记录")
    # 计算 ground truth
    print("🔍 计算 Ground Truth（最近邻）...")
    gt_indices = compute_ground_truth(train_vectors, test_vectors, top_k=top_k)
    save_ground_truth(os.path.join(output_dir, 'neighbors.parquet'), gt_indices)
    # 加载并保存标签文件（如果有）
    if label_path:
        print("📥 加载标签文件...")
        label_df = pd.read_csv(label_path)
        if 'labels' not in label_df.columns:
            raise ValueError("标签文件中必须包含 'labels' 列")
        label_df['labels'] = label_df['labels'].apply(literal_eval)
        label_df.to_parquet(os.path.join(output_dir, 'scalar_labels.parquet'), index=False)
        print("✅ 标签文件已保存为 scalar_labels.parquet")
if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="将CSV/NPY向量转换为VectorDBBench数据格式 (保留所有列)")
    parser.add_argument("--train", required=True, help="训练数据路径（CSV 或 NPY）")
    parser.add_argument("--test", required=True, help="查询数据路径（CSV 或 NPY）")
    parser.add_argument("--out", required=True, help="输出目录")
    parser.add_argument("--labels", help="标签CSV路径（可选）")
    parser.add_argument("--topk", type=int, default=10, help="Ground truth")
    args = parser.parse_args()
    main(args.train, args.test, args.out, args.labels, args.topk)

格式转换脚本-768M parquent到fbin
Cohere 1M vectors, 768 dimension
 cat parquet_to_fbin.py
import pandas as pd
import numpy as np
import os
import sys

def save_fbin(path, data):
    """将向量列表保存为DiskANN兼容的.fbin格式"""
    with open(path, "wb") as f:
        # 写入文件头：向量数量和维度
        f.write(np.array([len(data), len(data[0])], dtype=np.int32).tobytes())
        # 写入向量数据
        f.write(np.vstack(data).astype(np.float32).tobytes())

def main():
    parquet_path = "shuffle_train.parquet"

    # 1. 读取Parquet文件并提取向量
    print(f"读取Parquet文件: {parquet_path}")
    df = pd.read_parquet(parquet_path)
    print(f"检测到列名: {df.columns.tolist()}")

    # 根据实际列名提取向量
    vector_column = 'emb'  # 根据实际列名调整
    vectors = df[vector_column].apply(np.array).values

    # 2. 验证维度一致性
    dim = len(vectors[0])
    assert all(len(v) == dim for v in vectors), f"向量维度不一致！应为{dim}维"
    print(f"数据验证: 共{len(vectors)}条{dim}维向量")

    # 3. 保存基础集 (Base)
    base_path = "cohere_base.fbin"
    with open(base_path, "wb") as f:
        f.write(np.array([len(vectors), dim], dtype=np.int32).tobytes())
        f.write(np.vstack(vectors).astype(np.float32).tobytes())
    print(f"已生成基础集: {base_path}")

    # 4. 随机划分Learn集和Query集 (90% : 10%)
    print("随机划分数据集...")
    indices = np.random.permutation(len(vectors))
    split_idx = int(len(vectors) * 0.9)  # 90%作为Learn集

    learn_vectors = [vectors[i] for i in indices[:split_idx]]
    query_vectors = [vectors[i] for i in indices[split_idx:]]

    # 5. 保存Learn集和Query集
    save_fbin("cohere_learn.fbin", learn_vectors)
    save_fbin("cohere_query.fbin", query_vectors)
    print(f"已生成学习集: cohere_learn.fbin ({len(learn_vectors)}条)")
    print(f"已生成查询集: cohere_query.fbin ({len(query_vectors)}条)")

if __name__ == "__main__":
    main()

 cohere 是通过vectordbbench界面选择的 cohere 1M 768向量下载的 下载路径为/tmp/vectordb_bench/dataset/cohere/cohere_medium_1m
 
 cohere_base.fbin  cohere_learn.fbin  cohere_query.fbin  neighbors.parquet  parquet_to_fbin.py  scalar_labels.parquet  shuffle_train.parquet  test.parquet
 
 
 python parquet_to_fbin.py
读取Parquet文件: shuffle_train.parquet
检测到列名: ['id', 'emb']
数据验证: 共1000000条768维向量
已生成基础集: cohere_base.fbin
随机划分数据集...
已生成学习集: cohere_learn.fbin (900000条)
已生成查询集: cohere_query.fbin (100000条)


 ./build/apps/utils/compute_groundtruth  --data_type float --dist_fn l2 --base_file ../dataset/cohere/cohere_learn.fbin --query_file  ../dataset/cohere/cohere_query.fbin --gt_file ../dataset/cohere/cohere_query_learn_gt100 --K 100
        Going to compute 100 NNs for 100000 queries over 900000 points in 768 dimensions using L2 distance fn.
        
     2.9G    cohere_base.fbin
2.6G    cohere_learn.fbin
293M    cohere_query.fbin
77M     cohere_query_learn_gt100



vectordbbench部署
https://github.com/milvus-io/milvus/blob/master/configs/milvus.yaml的配置文件




python run.py --generate --engine infinity --dataset enwiki
python run.py --import --engine infinity --dataset enwiki
python run.py --query=16 --engine infinity --dataset enwiki
python run.py --query-express=16 --engine infinity --dataset enwiki



