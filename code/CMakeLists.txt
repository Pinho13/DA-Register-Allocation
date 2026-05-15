cmake_minimum_required(VERSION 3.17)
project(DA_Register_Allocation)

set(CMAKE_CXX_STANDARD 17)

include_directories(include)
include_directories(.)

add_executable(register_alloc
    src/main.cpp
    src/ui/Menu.cpp
    src/ui/DisplayFormatter.cpp
    src/parser/RangesParser.cpp
    src/parser/RegistersParser.cpp
    src/core/WebBuilder.cpp
    src/core/InterferenceGraph.cpp
    src/core/RegisterAllocator.cpp
    src/io/OutputWriter.cpp
    src/io/BatchProcessor.cpp
)
