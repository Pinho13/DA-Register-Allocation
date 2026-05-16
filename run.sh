#!/bin/bash 

mkdir -p code/cmake-build && 
cd code/cmake-build &&
cmake .. &&
make &&
cd ../.. &&
./code/cmake-build/register_alloc
