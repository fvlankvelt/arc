cmake_minimum_required(VERSION 3.18 FATAL_ERROR)
project(arc)

list(APPEND CMAKE_PREFIX_PATH "/opt/libtorch")
set(ENV{CUDACXX} "/usr/local/cuda-13.2/bin/nvcc")

find_package(Torch REQUIRED)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${TORCH_CXX_FLAGS}")

find_package(cJSON REQUIRED)

add_subdirectory(src)
add_subdirectory(test)
