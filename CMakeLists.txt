#find_package(OpenCV REQUIRED)
#include_directories($(OpenCV_INCLUDE_DIRS))
cmake_minimum_required(VERSION 3.1)
SET(CMAKE_BUILD_TYPE "Debug")  
SET(CMAKE_CXX_FLAGS_DEBUG "$ENV{CXXFLAGS} -O0 -Wall -g2 -ggdb")  
SET(CMAKE_CXX_FLAGS_RELEASE "$ENV{CXXFLAGS} -O3 -Wall")  


#find_package(OpenCV REQUIRED)
#include_directories($(OpenCV_INCLUDE_DIRS))
project (resize)
set(CMAKE_CXX_STANDARD 11)

include_directories (/mnt/data/GuanZH/resize)

add_executable(resize main.c Scale.c filter.c slice.c scale_v.c convert.c scale_h.c function.c)
target_link_libraries(resize m)


set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/build)