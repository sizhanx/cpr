CC=g++
FLAGS=-std=c++14 -g -Wall
OUT=*.o cp_r
TEST=buff_alloc_test user_data_test

cp_r: user_data buff_alloc cp_r.cpp
	$(CC) $(FLAGS) -o cp_r cp_r.cpp buff_alloc.o user_data.o -luring -lpthread

clean: 
	-rm -rf $(OUT) $(TEST) dest

user_data: user_data.cpp user_data.hpp
	$(CC) $(FLAGS) -o user_data.o -c user_data.cpp

user_data_test: clean user_data user_data.cpp
	$(CC) $(FLAGS) -o user_data_test user_data_test.cpp user_data.o && \
	./user_data_test

buff_alloc: buff_alloc.cpp buff_alloc.hpp
	$(CC) $(FLAGS) -o buff_alloc.o -c buff_alloc.cpp

buff_alloc_test: clean buff_alloc buff_alloc_test.cpp
	$(CC) $(FLAGS) -o buff_alloc_test buff_alloc_test.cpp buff_alloc.o && \
	valgrind --tool=memcheck ./buff_alloc_test

gdb: clean cp_r
	gdb --args ./cp_r test dest

valgrind: clean cp_r
	valgrind --tool=memcheck ./cp_r test dest

valgrind_full: clean cp_r
	valgrind --tool=memcheck --leak-check=full ./cp_r test dest

run: clean cp_r
	./cp_r test dest
