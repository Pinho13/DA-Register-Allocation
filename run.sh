#!/bin/bash 

if [ "$#" -eq 0 ]; then
    mkdir -p code/cmake-build &&
    mkdir -p outputs &&
    cd code/cmake-build &&
    cmake .. &&
    make &&
    cd ../.. &&
    ./code/cmake-build/register_alloc
elif [ "$#" -eq 4 ] && [ "$1" = "-b" ]; then
    mkdir -p code/cmake-build &&
    mkdir -p outputs &&
    cd code/cmake-build &&
    cmake .. &&
    make &&
    cd ../.. &&
    ./code/cmake-build/register_alloc -b "$2" "$3" "$4"
elif [ "$#" -eq 5 ] && [ "$1" = "-b" ]; then
    mkdir -p code/cmake-build &&
    mkdir -p outputs &&
    cd code/cmake-build &&
    cmake .. &&
    make &&
    cd ../.. &&
    ./code/cmake-build/register_alloc -b "$2" "$3" "$4" "$5"
else
    echo "Usage:"
    echo "  $0"
    echo "  $0 -b <ranges_full_path> <registers_full_path> <output_full_path>"
    echo "  $0 -b <dataset_type> <ranges_filename> <registers_filename> <output_filename>"
    exit 1
fi