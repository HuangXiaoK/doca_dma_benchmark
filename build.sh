#!/bin/bash
mkdir -p build
cd build
rm -rf *
cmake -DCMAKE_BUILD_TYPE=Release ../
mkdir -p bin
make
mv remote* ./bin/
mv local* ./bin/