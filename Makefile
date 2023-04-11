out = cpr

all: $(out)

clean: 
	rm $(out)

io_uring: io_uring.c io_uring.h
	gcc -o io_uring.o -c io_uring.c

cpr: io_uring cpr.c 
	gcc -g -o cpr cpr.c io_uring.o