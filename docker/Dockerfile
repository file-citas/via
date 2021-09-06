FROM ubuntu:20.04
ARG NT=1
RUN : \
    && apt-get update \
    && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
        software-properties-common \
    && add-apt-repository -y ppa:deadsnakes \
    && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
        python3.8 libconfig-dev cmake git gcc g++ make libllvm10 \
        bc libncurses-dev gawk flex bison openssl libssl-dev dkms libelf-dev libudev-dev libpci-dev libiberty-dev autoconf python \
        python-numpy python3-numpy \
    && apt-get clean \
    && rm -rf /var/lib/apt/lists/* \
    && :

WORKDIR /
RUN git clone https://github.com/file-citas/via.git via
RUN mkdir /via/llvm/build
RUN mkdir /via/fuzz/bin
RUN mkdir /via/fuzz/bin/harness
RUN mkdir /via/fuzz/targets_fuzz

WORKDIR /via/llvm/build
RUN cmake -DLLVM_ENABLE_PROJECTS="clang;compiler-rt" -DCMAKE_BUILD_TYPE="Release" -G "Unix Makefiles" ../llvm
RUN make -j $NT
RUN make install -j $NT
ADD llvm-symbolizer /usr/local/bin/llvm-symbolizer

ENV VIA_PATH /via
ENV VIA_PG_PATH $VIA_PATH/fuzz
ENV VIA_LKL_PATH $VIA_PATH/lkl
ENV CC clang
ENV CPP clang++
ENV LD_LIBRARY_PATH $VIA_LKL_PATH/tools/lkl/lib:$VIA_PG_PATH/bin
ENV VIA_MODULES $VIA_PG_PATH/targets_fuzz
ENV VIA_CONFIGS $VIA_PG_PATH/configs
ENV VIA_HARNESS $VIA_PG_PATH/bin/harness
ENV VIA_FW $VIA_PG_PATH/fw
ENV ASAN_OPTIONS detect_leaks=0
ENV VIA_BEVAL_DIR $VIA_PATH/benchmarks/eval
ENV VIA_BRESULT_DIR $VIA_PATH/benchmarks/results

WORKDIR /via/lkl
RUN make -C tools/lkl CC=$CC HOSTCC=$CC FUZZ=1 LLVM_IAS=1 PYTHON=python3.8 -j $NT
RUN make drivers modules ARCH=lkl CC=$CC HOSTCC=$CC FUZZ=1 LLVM_IAS=1 PYTHON=python3.8 -j $NT
RUN find . -name '*ko' -exec cp {} /via/fuzz/targets_fuzz \;

WORKDIR /via/fuzz
RUN make -j $NT

WORKDIR /
