out = cpr
cc = g++

all: $(out)

clean: 
	rm $(out)


cpr: cpr.cpp
	$(cc) -g -o cpr_cpp cpr.cpp -luring -lpthread

bitmap_test: bitmap.c bitmap.h
	$(cc) -g -o bitmap_test bitmap.c

bitmap: bitmap.c bitmap.h
	$(cc) -g -o bitmap.o -c bitmap.c
