.POSIX:
CXX = g++
CXXFLAGS = -O3 -fno-rtti -funroll-loops -DNDEBUG -std=c++11 -Wall -Wpedantic
FASTFLAGS = -march=native
bin/11mm: bin src/main.cpp src/asciiwarn.hpp
	$(CXX) $(CXXFLAGS) src/main.cpp -o bin/11mm
	strip bin/11mm

fast: set.mm bin src/main.cpp src/asciiwarn.hpp
	$(CXX) $(CXXFLAGS) $(FASTFLAGS) -fprofile-generate src/main.cpp -o 11mm_pg
	./11mm_pg set.mm > /dev/null
	$(CXX) $(CXXFLAGS) $(FASTFLAGS) -fprofile-use src/main.cpp -o 11mm_pg
	mv 11mm_pg bin/11mm
	strip bin/11mm
	rm -f 11mm_pg 11mm_pg-main.gcda
bin:
	mkdir -p bin
clean:
	rm -f bin/11mm
