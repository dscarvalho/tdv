all: setup_libs build download_data

setup_libs:
	./setup_libs.sh

build:
	$(MAKE) -C src/
	rm -rf build
	mkdir -p build/bin
	cp src/service build/bin/
	cp run_service.sh build/
	cp -r lib build/
	cp -r cfg build/
	mkdir build/data

	echo '#!/bin/bash' > build/run_service.sh
	ifeq ($(UNAME_S),Linux)
		echo 'LD_LIBRARY_PATH=$LD_LIBRARY_PATH:./lib ./bin/service -c ./cfg/service.conf' >> build/run_service.sh
	endif
	ifeq ($(UNAME_S),Darwin)
		echo 'DYLD_LIBRARY_PATH=$DYLD_LIBRARY_PATH:./lib ./bin/service -c ./cfg/service.conf' >> build/run_service.sh
	endif

download_data:
	cd build/data
	curl -O "http://www.jaist.ac.jp/~s1520009/files/tdv/enwiktdb_sorted_min.json.bz2"
	bunzip2 enwiktdb_sorted_min.json.bz2
	cd ../../

