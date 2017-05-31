UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Linux)
	CXXFLAGS = -std=c++11 -O3
endif
ifeq ($(UNAME_S),Darwin)
	CXXFLAGS = -std=c++11 -stdlib=libc++ -O3
endif

INCLUDE = -I ../include/ -I ../include/cppcms/

all: service gen_examples

clean:
	rm -f *.o service gen_examples

gen_vectors: stringutils.o config.o wiktdb.o sparsearray.o vectorize.o gen_vectors.o
	$(CXX) -L. -L"$(CURDIR)/../lib"  stringutils.o config.o wiktdb.o sparsearray.o vectorize.o gen_vectors.o -o gen_vectors

service: stringutils.o config.o wiktdb.o sparsearray.o vectorize.o service.o
	$(CXX) -L. -L"$(CURDIR)/../lib" -Wl,-rpath,"$(CURDIR)/../lib" -lcppcms stringutils.o config.o wiktdb.o sparsearray.o vectorize.o service.o -o service

gen_vectors.o: gen_vectors.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDE) -c gen_vectors.cpp -o gen_vectors.o

service.o: service.h service.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDE) -c service.cpp -o service.o

wiktdb.o: wiktdb.h wiktdb.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDE) -c wiktdb.cpp -o wiktdb.o

vectorize.o: vectorize.h vectorize.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDE) -c vectorize.cpp -o vectorize.o

sparsearray.o: sparsearray.h sparsearray.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDE) -c sparsearray.cpp -o sparsearray.o

stringutils.o: vectorutils.h stringutils.h stringutils.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDE) -c stringutils.cpp -o stringutils.o

config.o: config.h config.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDE) -c config.cpp -o config.o

mkbuild: 
	mkdir -p build
	mv *.o build/
