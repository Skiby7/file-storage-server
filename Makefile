CC		=  gcc
CFLAGS	+= -Wall -pedantic -std=c99
TARGETS = 

.PHONY: clean
.SUFFIXES: .c .h


bin/server: src/server.c parser.o
		$(CC) $(CFLAGS) src/server.c -o bin/server -L build -lparser -lpthread

parser.o: src/parser.c src/parser.h
		$(CC) $(CFLAGS) -c src/parser.c -o build/parser.o 
		ar rvs build/libparser.a build/parser.o
		

clean: 
	$(RM) build/* src/*.h.gch bin/server