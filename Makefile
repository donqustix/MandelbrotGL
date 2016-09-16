.PHONY: all run

all: src/main.cpp
	g++ -std=c++14 -pedantic -Wall -Wextra src/main.cpp -o bin/test -lSDL2 -lGLEW -lGL

run:
	./bin/test
