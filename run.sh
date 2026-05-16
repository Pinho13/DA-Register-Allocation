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
    ./code/cmake-build/register_alloc -b "dataset/$2" "dataset/$3" "outputs/$4"
else
    echo "Usage:"
    echo "  $0"
    echo "  $0 -b <ranges_path> <registers_path> <output_path>"
    exit 1
fi