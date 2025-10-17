FROM ubuntu:22.04

# 安装开发环境
RUN apt update && apt install -y \
    build-essential cmake gdb git vim curl \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /workspace
