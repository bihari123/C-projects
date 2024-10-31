cmake_minimum_required (VERSION 3.16)
project (myproj VERSION 1.0)

find_package (glog  REQUIRED)

add_executable (myapp src/main.cpp)
target_link_libraries (myapp glog::glog)
