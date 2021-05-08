#include "server.h"
#include "parser.h"

config configuration; // Server config
bool can_accept = true;


int main(int argc, char* argv[]){ // REMEMBER FFLUSH FOR THREAD PRINTF


	
	char SOCKETADDR[UNIX_MAX_PATH];
	struct sockaddr_un sockaddress;
	
	int socket_fd = 0, com = 0, i = 0; 
	unsigned int seed = time(NULL);
	init(SOCKETADDR); // Configuration struct is now initialized
	pthread_t *workers = (pthread_t *) malloc(configuration.workers*sizeof(pthread_t));
	memset(workers, 0, configuration.workers*sizeof(pthread_t));
	printconf();
	puts(SOCKETADDR);
	strncpy(sockaddress.sun_path, SOCKETADDR, UNIX_MAX_PATH);
	
	sockaddress.sun_family = AF_UNIX;
	socket_fd = socket(AF_UNIX, SOCK_STREAM, 0);
	unlink(SOCKETADDR);
	bind(socket_fd, (struct sockaddr *) &sockaddress, sizeof(sockaddress));
	listen(socket_fd, 10);
	while(true){
		com = accept(socket_fd, NULL, 0);
		if(can_accept){
			i = rand_r(&seed)%configuration.workers;
			pthread_create(&workers[i], NULL, conneciton_handler, (void *) com);
			pthread_detach(workers[i]);
		}
	}
	close(socket_fd);
	free(workers);
	return 0;

}

void* conneciton_handler(void *args){
	int com = (int) args;


	close(com);
}
	
	

void printconf(){
	printf("Workers: %d\nMem: %d\nFiles: %d\nSockname: %s\nLog: %s\n", configuration.workers, configuration.mem, configuration.files, configuration.sockname, configuration.log);
}
void init(char *sockname){
	
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
	memset(sockname, 0 , UNIX_MAX_PATH);
	sprintf(sockname, "/tmp/");
	strncat(sockname, configuration.sockname, strlen(configuration.sockname));

}