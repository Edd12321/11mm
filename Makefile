CXX = g++
CXXFLAGS = -O3 -march=native -std=c++11 -Wall -Wpedantic
bin/11mm: bin src/main.cpp
	$(CXX) $(CXXFLAGS) src/main.cpp -o bin/11mm
	strip bin/11mm
bin:
	mkdir -p bin
clean:
	rm -f bin/11mm
