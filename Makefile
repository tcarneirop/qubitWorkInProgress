CXX = g++

CXXFLAGS = -fopenmp -DSABRE -std=c++17 -O3

TARGETD = depth_qubit.exe
TARGETG = gates_qubit.exe

DEBUG_TARGETD = DEBUG_depth_qubit.exe
DEBUG_TARGETG = DEBUG_gates_qubit.exe

SOURCE = qubit_bitset.cpp

.PHONY: all gates depth clean

all: gates depth

solscounter: solsgates solsdepth


depth:
	$(CXX) $(CXXFLAGS) -DODEPTH $(SOURCE) -o $(TARGETD)

gates:
	$(CXX) $(CXXFLAGS) -DOGATES $(SOURCE) -o $(TARGETG)


solsgates:
	$(CXX) $(CXXFLAGS) -DOGATES -DSOLREPORTGATES  $(SOURCE)  -o $(DEBUG_TARGETG)

solsdepth:
	$(CXX) $(CXXFLAGS) -DODEPTH -DSOLREPORTDEPTH  $(SOURCE) -o $(DEBUG_TARGETD)

clean:
	rm -f *.o *.exe