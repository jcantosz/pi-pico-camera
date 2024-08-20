#!/bin/bash

# Ensure running in the same dir as build.sh
cd $(dirname $0)

# Create build dir if not exist
mkdir -p build

# Generate the build system
cmake -S . -B build

# Build the project
cd build
make -j16
