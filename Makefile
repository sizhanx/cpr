OUT=cp_r buf_alloc.o
CC=g++
FLAGS=-std=c++11 -g

all: $(OUT)

clean: 
	-rm $(OUT)

buf_alloc: buf_alloc.h buf_alloc.c
	$(CC) $(FLAGS) -o buf_alloc.o -c buf_alloc.cpp
	
cp_r: buf_alloc cp_r.cpp
	$(CC) $(FLAGS) -o cp_r cp_r.cpp -luring -lpthread


