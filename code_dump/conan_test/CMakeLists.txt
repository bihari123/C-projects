cmake_minimum_required(VERSION 3.15)
project(MyProject CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Set the path to the Conan-generated files
set(CONAN_GENERATORS_FOLDER ${CMAKE_BINARY_DIR}/generators)

# Debug information
message(STATUS "CONAN_GENERATORS_FOLDER: ${CONAN_GENERATORS_FOLDER}")
message(STATUS "CMAKE_MODULE_PATH before: ${CMAKE_MODULE_PATH}")
message(STATUS "CMAKE_PREFIX_PATH before: ${CMAKE_PREFIX_PATH}")

# Add the Conan generators folder to the module and prefix paths
list(APPEND CMAKE_MODULE_PATH ${CONAN_GENERATORS_FOLDER})
list(APPEND CMAKE_PREFIX_PATH ${CONAN_GENERATORS_FOLDER})

message(STATUS "CMAKE_MODULE_PATH after: ${CMAKE_MODULE_PATH}")
message(STATUS "CMAKE_PREFIX_PATH after: ${CMAKE_PREFIX_PATH}")

include_directories(${CMAKE_SOURCE_DIR}/include)

# Find the fmt package
find_package(fmt REQUIRED)

# Debug: Print fmt package information if found
if(fmt_FOUND)
    message(STATUS "fmt package found:")
    message(STATUS "  version: ${fmt_VERSION}")
    message(STATUS "  include dirs: ${fmt_INCLUDE_DIRS}")
    message(STATUS "  libraries: ${fmt_LIBRARIES}")
else()
    message(FATAL_ERROR "fmt package not found")
endif()

add_executable(${PROJECT_NAME} src/main.cpp)
target_link_libraries(${PROJECT_NAME} PRIVATE fmt::fmt)
