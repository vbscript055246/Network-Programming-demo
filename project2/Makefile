CXX=g++
CXXFLAGS=-std=c++14 -Wall
CXX_INCLUDE_DIRS=/usr/local/include
CXX_INCLUDE_PARAMS=$(addprefix -I , $(CXX_INCLUDE_DIRS))
CXX_LIB_DIRS=/usr/local/lib
CXX_LIB_PARAMS=$(addprefix -L , $(CXX_LIB_DIRS))

all: np_simple.cpp np_single_proc.cpp np_multi_proc.cpp
	$(CXX) np_simple.cpp -o np_simple $(CXX_INCLUDE_PARAMS) $(CXX_LIB_PARAMS) $(CXXFLAGS)
	$(CXX) np_single_proc.cpp -o np_single_proc $(CXX_INCLUDE_PARAMS) $(CXX_LIB_PARAMS) $(CXXFLAGS)
	$(CXX) np_multi_proc.cpp -o np_multi_proc $(CXX_INCLUDE_PARAMS) $(CXX_LIB_PARAMS) $(CXXFLAGS)
clean:
	rm -f np_simple np_single_proc np_multi_proc