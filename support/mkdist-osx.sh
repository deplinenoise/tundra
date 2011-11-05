#! /bin/sh

# Utility script to bootstrap, build and upload a full binary distribution of
# Tundra to github for downloading.
#
# Usage: support/mkdist-osx.sh <tag> [upload]
#
# Tag should be a version number, e.g. 0.99f
# Upload is the optional literal "upload", if not specified the build is local.

MONIKER=$1
UPLOAD=$2

if [ x$MONIKER = x ]; then
	echo "error: specify a moniker"
	exit 1
fi

if [ x$UPLOAD = x ]; then
    UPLOAD=no
elif [ x$UPLOAD = xupload ]; then
    UPLOAD=yes
else
    echo "error: specify 'upload' for the second arg if desired"
	exit 1
fi

test -d dists || mkdir dists
DISTOSX=tundra-$MONIKER-osxintel64
DISTWIN=tundra-$MONIKER-win32
DISTMAN=tundra-$MONIKER-manual

test -f tundra.lua || exit 1

find examples -name tundra-output -exec rm -rf {} \;
find . -name .tundra-\* -exec rm -f {} \;

rm -rf build $DISTOSX $DISTWIN tundra-output

mkdir build
cd build
cmake .. || exit 1
make || exit 1
cd ..

TUNDRA_HOME=$PWD build/tundra standalone release macosx-clang || exit 1
TUNDRA_HOME=$PWD build/tundra standalone release macosx-crosswin32 || exit 1

mkdir $DISTOSX
mkdir $DISTOSX/doc
cp -r README.md COPYING examples $DISTOSX
cp doc/manual.asciidoc $DISTOSX/doc
git log -1 >> $DISTOSX/SNAPSHOT_REVISION
find $DISTOSX -name \*.swp -exec rm {} \;
find $DISTOSX -name .DS_Store -exec rm {} \;

cp -r $DISTOSX $DISTWIN

cp tundra-output/macosx-clang-release-standalone/tundra $DISTOSX || exit 1
cp tundra-output/macosx-crosswin32-release-standalone/tundra.exe $DISTWIN || exit 1

tar cjvf dists/$DISTOSX.tar.bz2 $DISTOSX || exit 1
zip -r dists/$DISTWIN.zip $DISTWIN || exit 1

rm -rf $DISTOSX $DISTWIN

make -C doc
cp doc/manual.pdf dists/$DISTMAN.pdf

REPO=deplinenoise/tundra

if [ x$UPLOAD = xyes ]; then
    echo "uploading files to $REPO..."
    support/github-upload.rb dists/$DISTOSX.tar.bz2 $REPO || exit 1
    support/github-upload.rb dists/$DISTWIN.zip $REPO || exit 1
    support/github-upload.rb dists/$DISTMAN.pdf $REPO || exit 1
fi

echo
echo "All done, tundra $MONIKER ready"
