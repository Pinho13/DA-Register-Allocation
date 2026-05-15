cmake_minimum_required(VERSION 3.17)
project(DA_Register_Allocation)

set(CMAKE_CXX_STANDARD 17)

include_directories(include)
include_directories(data_structures)

add_executable(register_alloc
    src/main.cpp
    src/Menu.cpp
    src/RangesParser.cpp
    src/RegistersParser.cpp
    src/WebBuilder.cpp
    src/InterferenceGraph.cpp
    src/RegisterAllocator.cpp
    src/OutputWriter.cpp
    src/DisplayFormatter.cpp
    src/BatchProcessor.cpp
)
