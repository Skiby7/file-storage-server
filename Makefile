CC	=  gcc
CFLAGS	+= -Wall -pedantic -std=c99 -g
# DEFINES += -D_CIAO=1
DEFINES += -D_GNU_SOURCE=1
INCLUDES = -I includes/
TARGETS = server client
LIBS = -L ./libs/zlib -lz
.PHONY: clean clean_files clean_all gen_files test1 test2 test3 test1_un test3_un
.SUFFIXES: .c .h


all: server client binary_test

server:  libserver libcommon zlib
	$(CC) $(CFLAGS) $(DEFINES) $(INCLUDES) src/server.c -o bin/server  -lpthread -L ./build -lserver -lcommon $(LIBS)

client: libclient libcommon
	$(CC) $(CFLAGS) $(DEFINES) $(INCLUDES) src/client.c -o bin/client -L ./build -lclient -lcommon

binary_test:
	$(CC) $(CFLAGS) $(DEFINES) $(INCLUDES) src/binary_test.c -o test/binary/binary_test

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
zlib:
	cd libs/zlib/ && ./configure --static --const && make -j

clean: 
	$(RM) build/* src/*.h.gch bin/client bin/server libs/zlib/libz.a

clean_files:
	$(RM) test/large_files/* test/medium_files/* test/small_files/* test/test_2/*

clean_all: clean clean_files
	
gen_files:
	./generate_file_init.sh
	

test1: client server
	$(RM) -r ./test/test_output/*
	$(RM) -r ./test/output_stress_test/*
	./test/test1.sh bin/config1.txt
	./statistiche.sh bin/server.log

test2: client server
	$(RM) -r ./test/test_output/*
	$(RM) -r ./test/output_stress_test/*
	./test/test2.sh bin/config2.txt
	./statistiche.sh bin/server.log

test3: client server
	$(RM) -r ./test/test_output/*
	$(RM) -r ./test/output_stress_test/*
	./test/test3.sh bin/config3txt
	./statistiche.sh bin/server.log


test1_un: client server
	$(RM) -r ./test/test_output/*
	$(RM) -r ./test/output_stress_test/*
	./test/test1.sh bin/config1_un.txt
	./statistiche.sh bin/server.log

test3_un: client server
	$(RM) -r ./test/test_output/*
	$(RM) -r ./test/output_stress_test/*
	./test/test3.sh bin/config3_un.txt
	./statistiche.sh bin/server.log