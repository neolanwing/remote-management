cmake_minimum_required(VERSION 3.10)
project(MyApp C)

# 指定 C 标准
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# 包含头文件目录
include_directories(${PROJECT_SOURCE_DIR}/inc)

# 自动收集 src/ 目录下所有 .c 文件
file(GLOB_RECURSE SOURCES src/*.c)

# 生成可执行文件
add_executable(remote ${SOURCES})
