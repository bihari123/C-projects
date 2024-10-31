cmake_minimum_required(VERSION 3.14)
project(
  emotion_analyzer
  VERSION 1.0.0
  LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

find_package(Drogon REQUIRED)
find_package(
  Python3
  COMPONENTS Development
  REQUIRED)
find_package(Threads REQUIRED)

# Set Python paths
execute_process(
  COMMAND python3-config --includes
  OUTPUT_VARIABLE PYTHON_INCLUDES
  OUTPUT_STRIP_TRAILING_WHITESPACE)
string(REPLACE "-I" "" PYTHON_INCLUDES ${PYTHON_INCLUDES})
string(REPLACE " " ";" PYTHON_INCLUDE_DIRS ${PYTHON_INCLUDES})

include_directories(${PROJECT_SOURCE_DIR}/include ${PYTHON_INCLUDE_DIRS})

set(SOURCES src/main.cpp src/controllers/EmotionAnalyzer.cpp
            src/controllers/EmotionAnalyzerController.cpp)

add_executable(${PROJECT_NAME} ${SOURCES})

target_link_libraries(${PROJECT_NAME} PRIVATE Drogon::Drogon Python3::Python
                                              Threads::Threads)

# Copy Python script and config
file(COPY ${PROJECT_SOURCE_DIR}/src/python/emotion_analyzer.py
     DESTINATION ${CMAKE_BINARY_DIR})
file(COPY ${PROJECT_SOURCE_DIR}/config/config.json
     DESTINATION ${CMAKE_BINARY_DIR})
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/uploads)
