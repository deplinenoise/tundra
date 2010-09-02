#! /bin/sh

test -f tundra.lua || exit 1

rm -rf build dist

mkdir build
cd build
cmake -G "Visual Studio 9 2008 Win64" ..
"/c/Program Files (x86)/Microsoft Visual Studio 9.0/Common7/IDE/devenv.com" Tundra.sln //build Release
cd ..

TUNDRA_HOME=$PWD build/Release/tundra standalone release win64-msvc

mkdir dist
mkdir dist/doc
cp -r README.md COPYING examples dist
cp doc/manual.asciidoc dist/doc
cp tundra-output/win64-msvc-release-standalone/tundra.exe dist
git log -1 >> dist/SNAPSHOT_REVISION
