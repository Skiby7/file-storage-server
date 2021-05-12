CC		=  gcc
CFLAGS	+= -Wall -pedantic -std=gnu99
TARGETS = server client

.PHONY: clean test1 test2 test3
.SUFFIXES: .c .h


all: server client

server:  libutils
		$(CC) $(CFLAGS) src/server.c -o bin/server  -lpthread -L ./build -lutils

client: libutils
		$(CC) $(CFLAGS) src/client.c -o bin/client -L ./build -lutils 

libutils: parser.o fssApi.o client_queue.o connections.o
		ar rvs build/libutils.a build/parser.o build/fssApi.o build/client_queue.o build/connections.o
		# gcc -shared -o build/libutils.so build/parser.o build/fssApi.o 
		rm build/parser.o
		rm build/fssApi.o
		rm build/client_queue.o
		rm build/connections.o

parser.o: 
		# $(CC) $(CFLAGS) -c -fPIC src/parser.c -o build/parser.o 
		$(CC) $(CFLAGS) -c src/parser.c -o build/parser.o 

fssApi.o:
		# $(CC) $(CFLAGS) -c -fPIC src/fssApi.c -o build/fssApi.o 
		$(CC) $(CFLAGS) -c src/fssApi.c -o build/fssApi.o 

client_queue.o:	
		$(CC) $(CFLAGS) -c src/client_queue.c -o build/client_queue.o 

connections.o:
		$(CC) $(CFLAGS) -c src/connections.c -o build/connections.o 
clean: 
	$(RM) build/* src/*.h.gch bin/client bin/server