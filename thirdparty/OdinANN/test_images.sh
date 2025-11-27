#!/bin/bash

# 设置错误处理
#set -e

# 容器名称
CONTAINER_NAME="pipeann-test-$(uuidgen)"

# 清理可能存在的旧容器
docker rm -f $CONTAINER_NAME 2>/dev/null || true

echo "正在创建并启动容器..."
# 创建并启动容器，执行命令后自动退出
docker run --name $CONTAINER_NAME \
  registry.interesting.com:80/interesting/pipeann \
  sh -c "export PATH=/root/PipeANN/build/tests:/root/PipeANN/build/tests/utils:\$PATH && search_disk_index -h"

echo "容器执行完成，输出已显示"

# 删除容器
echo "正在删除容器..."
docker rm $CONTAINER_NAME

echo "测试完成，容器已删除"
