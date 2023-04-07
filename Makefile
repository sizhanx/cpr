out = cpr

all: $(out)

clean: 
	rm $(out)

cpr: cpr.c
	gcc -g -o cpr cpr.c