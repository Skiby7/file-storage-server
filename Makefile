CC		=  gcc
CFLAGS	+= -Wall -pedantic -std=c99 -g
DEFINES += -D_GNU_SOURCE=1
INCLUDES = -I includes/
TARGETS = server client
LIBS = -L ./libs -lz
.PHONY: clean test1 test2 test3
.SUFFIXES: .c .h


all: server client

server:  libserver libcommon
		$(CC) $(CFLAGS) $(DEFINES) $(INCLUDES) src/server.c -o bin/server  -lpthread -L ./build -lserver -lcommon $(LIBS)

client: libclient libcommon
		$(CC) $(CFLAGS) $(DEFINES) $(INCLUDES) src/client.c -o bin/client -L ./build -lclient -lcommon

libserver: parser.o client_queue.o log.o file.o
		ar rvs build/libserver.a build/parser.o  build/client_queue.o build/log.o build/file.o
		rm build/parser.o
		rm build/client_queue.o
		rm build/log.o
		rm build/file.o

libclient: work.o fssApi.o 
		ar rvs build/libclient.a build/work.o build/fssApi.o
		rm build/work.o
		rm build/fssApi.o

libcommon: connections.o serialization.o
		ar rvs build/libcommon.a build/connections.o build/serialization.o	
		rm build/serialization.o
		rm build/connections.o

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

serialization.o:
		$(CC) $(CFLAGS) $(DEFINES) $(INCLUDES) -c src/serialization.c -o build/serialization.o

work.o: 
		$(CC) $(CFLAGS) $(DEFINES) $(INCLUDES) -c src/work.c -o build/work.o

clean: 
		$(RM) build/* src/*.h.gch bin/client bin/server

test1: client server
	$(RM) ./test/test_output/*
	./test/test1.sh
	./statistiche.sh bin/server.log

test2: client server
	$(RM) -r ./test/test_output/*
	./test/test2.sh
	./statistiche.sh bin/server.log
