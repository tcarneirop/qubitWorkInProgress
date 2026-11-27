
CXX = g++
CXXFLAGS = -O3 -std=c++17 -fopenmp -I../../qiskit_c
LDFLAGS = -fopenmp

TARGET = grasp_exe

SRCS = grasp_exe.cpp routing_core.cpp
OBJS = $(SRCS:.cpp=.o)

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(OBJS) $(LDFLAGS) -o $@

grasp_exe.o: grasp_exe.cpp grasp.hpp grasp_graph.hpp routing_core.hpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

routing_core.o: routing_core.cpp routing_core.hpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)
