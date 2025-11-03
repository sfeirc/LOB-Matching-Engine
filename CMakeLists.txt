cmake_minimum_required(VERSION 3.20)
project(LOB CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# NUCLEAR BUILD - Maximum optimization flags
set(CMAKE_BUILD_TYPE Release)
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -march=native -flto -ffast-math -funroll-loops -fomit-frame-pointer")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -finline-functions -fno-strict-aliasing")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -DNDEBUG -m64 -mtune=native")
# Link-time optimization
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)

# Include directories
include_directories(include)

# Source files
set(SOURCES
    src/main.cpp
    src/OrderBook.cpp
    src/CSVReader.cpp
)

# Header files
set(HEADERS
    include/OrderBook.h
    include/Message.h
    include/Trade.h
    include/CSVReader.h
)

# Create executables
add_executable(replay ${SOURCES} ${HEADERS})

# Generator executable
add_executable(generate_dataset src/generate_dataset.cpp)

# Unit tests executable
add_executable(test_orderbook tests/test_orderbook.cpp src/OrderBook.cpp)
target_link_libraries(test_orderbook)

# Build command message
message(STATUS "Build with: cmake --build . -j")
message(STATUS "Run with: ./replay ../data/sample.csv")
message(STATUS "Generate dataset: ./generate_dataset [num_messages]")

