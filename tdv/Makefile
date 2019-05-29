UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Linux)
	SERVICE_CMD = 'LD_LIBRARY_PATH=$$LD_LIBRARY_PATH:./lib ./bin/service -c ./cfg/service.conf';
endif
ifeq ($(UNAME_S),Darwin)
	SERVICE_CMD = 'DYLD_LIBRARY_PATH=$$DYLD_LIBRARY_PATH:./lib ./bin/service -c ./cfg/service.conf'
endif

all: build download_data

lib:
	./setup_libs.sh

build: lib
	$(MAKE) -C src/
	rm -rf build
	mkdir -p build/bin
	cp src/service build/bin/
	cp src/gen_vectors build/bin/
	cp -r lib build/
	cp -r cfg build/
	mkdir -p build/data

	echo '#!/bin/bash' > build/run_service.sh
	echo $(SERVICE_CMD) >> build/run_service.sh
	chmod +x build/run_service.sh
	

download_data:
	cd build/data
	curl -o build/data/enwiktdb_sorted_min.json.bz2 "http://labcores.ppgi.ufrj.br/files/tdv/enwiktdb_sorted_min.json.bz2"
	bunzip2 build/data/enwiktdb_sorted_min.json.bz2

