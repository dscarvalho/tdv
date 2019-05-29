#!/bin/bash

mkdir libs
cd libs
svn co https://svn.code.sf.net/p/cppcms/code/framework/trunk cppcms_lib
mkdir -p cppcms_lib/build
cd cppcms_lib/build
sed -i 's/set(BOOSTER_SOVERSION "0")/set(BOOSTER_SOVERSION "0")\nset(CMAKE_CXX_FLAGS "-std=c++11 -stdlib=libc++ -O3 -Wall")\nset(CMAKE_EXE_LINKER_FLAGS "-liconv")/g' ../CMakeLists.txt
cmake -DCMAKE_INSTALL_PREFIX=../../cppcms ..
make
make install
cd ../../../
mkdir -p lib
cp -r libs/cppcms/lib/* lib/
mkdir -p include/cppcms
cp -r libs/cppcms/include/* include/cppcms/
rm -rf libs

