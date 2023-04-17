OUT=cp_r buff_alloc.o
CC=g++
FLAGS=-std=c++11 -g

all: $(OUT)

clean: 
	-rm -rf $(OUT) dest

buff_alloc: buff_alloc.cpp buff_alloc.hpp
	$(CC) $(FLAGS) -o buff_alloc.o -c buff_alloc.cpp
	
cp_r: buff_alloc cp_r.cpp
	$(CC) $(FLAGS) -o cp_r cp_r.cpp buff_alloc.o -luring -lpthread


