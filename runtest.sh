#!/bin/bash
set -e

rm -rf out_test_linux
mkdir out_test_linux
cd out_test_linux

echo Configuring...
CXX=${CXX:-g++} cmake ../ -G Ninja

echo Building...
cmake --build .

echo Running CTest...
ctest --output-on-failure

echo Running gcovr...
gcovr --root "../" --exclude ".*/catch.hpp" --exclude ".*/test_compile_file/.*" --exclude ".*/test/.*" --sort-percentage --html-details "results.html" .
