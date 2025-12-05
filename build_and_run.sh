#!/bin/bash

# Create build directory if it doesn't exist
if [ ! -d "build" ]; then
  mkdir build
fi

# Go into build directory
cd build

# Run CMake
cmake ..

# Build the project
make

# Run the executable if build was successful
if [ $? -eq 0 ]; then
  echo "Build successful! Running application..."
  echo "----------------------------------------"
  ./amalyzer "../audio.mp3"
else
  echo "Build failed."
fi
