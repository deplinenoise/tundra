#! /bin/sh

test -f tundra.lua || exit 1

find examples -name tundra-output -exec rm -rf {} \;
find examples -name .tundra-\* -exec rm -f {} \;

rm -rf build dist

mkdir build
cd build
cmake ..
make
cd ..

TUNDRA_HOME=$PWD build/tundra standalone release macosx-clang

mkdir dist
mkdir dist/doc
cp -r README.md COPYING examples dist
cp doc/manual.asciidoc dist/doc
cp tundra-output/macosx-clang-release-standalone/tundra dist
git log -1 >> dist/SNAPSHOT_REVISION
find dist -name \*.swp -exec rm {} \;
find dist -name .DS_Store -exec rm {} \;

