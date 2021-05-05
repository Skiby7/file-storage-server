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
#define MAX_BUFFER_LEN 151

int init(){
	char *buff = (char *) malloc(MAX_BUFFER_LEN);
	memset(buff, 0, MAX_BUFFER_LEN);
	FILE *config = NULL;
	if((config = fopen("config.txt", "r")) == NULL){

	}

	
}

int main(int argc, char* argv[]){


}