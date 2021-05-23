CC		=  gcc
CFLAGS	+= -Wall -pedantic -std=c99 -g
DEFINES += -D_GNU_SOURCE=1
INCLUDES = -I includes/
TARGETS = server client

.PHONY: clean test1 test2 test3
.SUFFIXES: .c .h


all: server client

server:  libutils
		$(CC) $(CFLAGS) $(DEFINES) $(INCLUDES) src/server.c -o bin/server  -lpthread -L ./build -lutils

client: libutils
		$(CC) $(CFLAGS) $(DEFINES) $(INCLUDES) src/client.c -o bin/client -L ./build -lutils 

libutils: parser.o fssApi.o client_queue.o connections.o log.o file.o
		ar rvs build/libutils.a build/parser.o build/fssApi.o build/client_queue.o build/connections.o build/log.o build/file.o
		rm build/parser.o
		rm build/fssApi.o
		rm build/client_queue.o
		rm build/connections.o
		rm build/log.o
		rm build/file.o



parser.o: 
		$(CC) $(CFLAGS) $(DEFINES) $(INCLUDES) -c src/parser.c -o build/parser.o 

fssApi.o:
		$(CC) $(CFLAGS) $(DEFINES) $(INCLUDES) -c src/fssApi.c -o build/fssApi.o 

client_queue.o:	
		$(CC) $(CFLAGS) $(DEFINES) $(INCLUDES) -c src/client_queue.c -o build/client_queue.o 

connections.o:
		$(CC) $(CFLAGS) $(DEFINES) $(INCLUDES) -c src/connections.c -o build/connections.o 

log.o:
		$(CC) $(CFLAGS) $(DEFINES) $(INCLUDES) -c src/log.c -o build/log.o 

file.o: 
		$(CC) $(CFLAGS) $(DEFINES) $(INCLUDES) -c src/file.c -o build/file.o




clean: 
		$(RM) build/* src/*.h.gch bin/client bin/server