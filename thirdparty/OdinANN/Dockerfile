FROM docker.m.daocloud.io/ubuntu:22.04

# 声明架构变量
ARG TARGETARCH
ARG TARGETVARIANT

RUN apt update
RUN apt install -y software-properties-common
RUN add-apt-repository -y ppa:git-core/ppa
RUN apt update
RUN DEBIAN_FRONTEND=noninteractive apt install -y git make cmake g++ libaio-dev libgoogle-perftools-dev libunwind-dev clang-format libboost-dev libboost-program-options-dev libmkl-full-dev libcpprest-dev python3.10 libboost-all-dev  libjemalloc-dev


#RUN export https_proxy=http://192.168.1.202:8889
#RUN git clone  https://github.com/interestingyong/PipeANN.git
WORKDIR /root/
COPY . /root/PipeANN/
#COPY -r ../PipeANN /root/ 
WORKDIR /root/PipeANN/

RUN cd third_party/liburing && \
    ./configure && \
    make -j$(nproc)

RUN ./build.sh

# git copy

