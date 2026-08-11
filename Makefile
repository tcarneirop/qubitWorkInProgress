CXX = g++
CXXFLAGS = -fopenmp -DSABRE -std=c++17 -O3

TARGET = qubit.exe
SOURCE = qubit_bitset.cpp

all: $(TARGET)

$(TARGET): $(SOURCE)
	$(CXX) $(CXXFLAGS) $(SOURCE) -o $(TARGET)

clean:
	rm -f $(TARGET)
