CXX = g++

CXXFLAGS = -fopenmp -DSABRE -std=c++17 -O3

TARGETD = depth_qubit.exe
TARGETG = gates_qubit.exe
SOURCE = qubit_bitset.cpp

.PHONY: all gates depth clean

all: gates depth
gates:
	$(CXX) $(CXXFLAGS) -DOGATES $(SOURCE) -o $(TARGETG)

depth:
	$(CXX) $(CXXFLAGS) -DODEPTH $(SOURCE) -o $(TARGETD)

clean:
	rm -f *.o *.exe