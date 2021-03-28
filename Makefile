CXX=g++
CXXFLAGS=-g -O0 -std=c++11

main: main.cc MCC.cc
	$(CXX) $(CXXFLAGS) -o $@ $^ -I.

test: main
	./$<

gdb: main
	gdb ./$<

.PHONY: clean
clean:
	rm -f main
