#! /bin/sh

test -f tundra.lua || exit 1

rm -rf build
mkdir build
cd build
cmake ..
make
cd ..
TUNDRA_HOME=$PWD build/tundra standalone release
