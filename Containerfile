# UBI9ベースのC/C++開発環境
FROM registry.access.redhat.com/ubi9/ubi:latest

# 必要なパッケージをインストール
RUN dnf update -y && \
    dnf install -y \
        cmake \
        gcc \
        gcc-c++ \
        glibc-devel \
        make \
        gdb \
        strace \
        vim \
        git \
        autoconf \
        automake \
        libtool \
        kernel-headers \
        diffutils \
        file \
        findutils \
        which \
    && dnf clean all

# 作業ディレクトリ設定
WORKDIR /workspace

# 共有メモリ用のディレクトリを作成
RUN mkdir -p /dev/shm && chmod 1777 /dev/shm

# プロジェクトファイルをコピー
COPY . /workspace/

# ビルドディレクトリ作成
RUN mkdir -p build

# デフォルトコマンド
CMD ["/bin/bash"]