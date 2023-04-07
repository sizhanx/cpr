out = cpr

all: $(out)

clean: 
	rm $(out)

cpr: cpr.c cpr.h
	gcc -g -o cpr cpr.c