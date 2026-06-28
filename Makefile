CXX = g++
CXXFLAGS = -O3
bin/11mm: src/main.cpp
	$(CXX) $(CXXFLAGS) src/main.cpp -o bin/11mm
