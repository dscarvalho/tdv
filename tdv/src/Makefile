CXXFLAGS = -std=c++11 -stdlib=libc++ -O3
INCLUDE = -I ../include/ -I ../include/cppcms/

all: service gen_examples

clean:
	rm -f *.o service gen_examples

gen_examples: stringutils.o config.o wiktdb.o sparsearray.o vectorize.o gen_examples.o
	$(CXX) -L ../lib  stringutils.o config.o wiktdb.o sparsearray.o vectorize.o gen_examples.o -o gen_examples

service: stringutils.o config.o wiktdb.o sparsearray.o vectorize.o service.o
	$(CXX) -L ../lib -lcppcms stringutils.o config.o wiktdb.o sparsearray.o vectorize.o service.o -o service

gen_examples.o: gen_examples.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDE) -c gen_examples.cpp -o gen_examples.o

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
