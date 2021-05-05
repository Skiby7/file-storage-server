#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <pthread.h>
#include <time.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/un.h>
#include "parser.h"

config configuration; // Server config

void printconf(){
	printf("Workers: %d\nMem: %d\nFiles: %d\nSockname: %s\nLog: %s\n", configuration.workers, configuration.mem, configuration.files, configuration.sockname, configuration.log);
}

void init(){
	
	FILE *conf = NULL;
	if((conf = fopen("bin/config.txt", "r")) == NULL){
		perror("Error while opening config file");
		exit(EXIT_FAILURE);
	}
	if(parseConfig(conf, &configuration) < 0){
		fprintf(stderr, ANSI_COLOR_RED "Error while parsing config.txt!\n" ANSI_COLOR_RESET);
		exit(EXIT_FAILURE);
	}
	fclose(conf);
}

int main(int argc, char* argv[]){
	init(); // Configuration struct is now initialized
	pthread_t *workers = (pthread_t *) malloc(configuration.workers*sizeof(pthread_t));
	memset(workers, 0, configuration.workers*sizeof(pthread_t));
	printconf();

	free(workers);
	return 0;

}