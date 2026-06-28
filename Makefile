CXX = g++
CXXFLAGS = -O3 -std=c++11
bin/11mm: src/main.cpp
	$(CXX) $(CXXFLAGS) src/main.cpp -o bin/11mm
clean:
	rm -f bin/11mm
