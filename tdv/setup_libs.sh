#!/bin/bash

mkdir libs
cd libs
svn co https://svn.code.sf.net/p/cppcms/code/framework/trunk cppcms_lib
mkdir -p cppcms_lib/build
cd cppcms_lib/build
cmake -DCMAKE_INSTALL_PREFIX=../../cppcms ..
make
make install
cd ../../../
mkdir -p lib
cp -r libs/cppcms/lib/* lib/
mkdir -p include/cppcms
cp -r libs/cppcms/include/* include/cppcms/
rm -rf libs

