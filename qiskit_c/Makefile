CXX = g++
CXXFLAGS = -O2 -std=c++17 -Wall -Wextra

QISKIT_HOME ?= $(HOME)/qubitWorkInProgress/qiskit

INCLUDES = -I$(QISKIT_HOME)/dist/c/include
LIBDIR = $(QISKIT_HOME)/dist/c/lib
LDFLAGS = -L$(LIBDIR) -Wl,-rpath,$(LIBDIR) -lqiskit

.PHONY: sanity clean
.PHONY: circuit
.PHONY: target
.PHONY: transpile
.PHONY: mapping
.PHONY: qasm

qasm: qasm_test.exe
	./qasm_test.exe test.qasm

qasm_test.exe: qasm_test.cpp qasm_parser.hpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) $< -o $@ $(LDFLAGS)

mapping: mapping_test.exe
	./mapping_test.exe

mapping_test.exe: mapping_test.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) $< -o $@ $(LDFLAGS)

transpile: transpile_test.exe
	./transpile_test.exe

transpile_test.exe: transpile_test.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) $< -o $@ $(LDFLAGS)
	
target: target_test.exe
	./target_test.exe

target_test.exe: target_test.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) $< -o $@ $(LDFLAGS)

circuit: circuit_test.exe
	./circuit_test.exe

circuit_test.exe: circuit_test.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) $< -o $@ $(LDFLAGS)

sanity: sanity_test.exe
	./sanity_test.exe

sanity_test.exe: sanity_test.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) $< -o $@ $(LDFLAGS)

clean:
	rm -f *.exe
