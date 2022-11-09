#!/bin/bash
mkdir -p build
cd build
rm -rf *
../cmake-3.16.8-Linux-x86_64/bin/cmake ../
mkdir -p bin
make
mv remote* ./bin/
mv local* ./bin/