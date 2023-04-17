OUT=cp_r buff_alloc.o user_data.o user_data.o
CC=g++
FLAGS=-std=c++14 -g -Wall

all: $(OUT)

clean: 
	-rm -rf $(OUT) dest

user_data: user_data.cpp user_data.hpp
	$(CC) $(FLAGS) -o user_data.o -c user_data.cpp

buff_alloc: buff_alloc.cpp buff_alloc.hpp
	$(CC) $(FLAGS) -o buff_alloc.o -c buff_alloc.cpp
	
cp_r: user_data buff_alloc cp_r.cpp
	$(CC) $(FLAGS) -o cp_r cp_r.cpp buff_alloc.o user_data.o -luring -lpthread

