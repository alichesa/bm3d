cmake_minimum_required(VERSION 3.8 FATAL_ERROR)
project(bm3d LANGUAGES CXX CUDA)

include_directories(
	//包含的头文件
	${CMAKE_CURRENT_SOURCE_DIR}/include  
	// 指定CUDA
	${CMAKE_CUDA_TOOLKIT_INCLUDE_DIRECTORIES}
)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# The following lines build the main executable.
add_executable(bm3d
	include/CImg.h
	include/stopwatch.hpp
	include/indices.cuh
	include/params.hpp
	include/bm3d.hpp
	src/filtering.cu
	src/blockmatching.cu
	src/dct8x8.cu
	src/main_nodisplay.cpp
)

target_link_libraries(bm3d cufft cudart png)
