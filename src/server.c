#include "server.h"
#include "parser.h"

config configuration; // Server config



int main(int argc, char* argv[]){
	char SOCKETADDR[UNIX_MAX_PATH];
	struct sockaddr_un sockaddress;
	pthread_t *workers = (pthread_t *) malloc(configuration.workers*sizeof(pthread_t));
	int socket_fd, com;
	
	init(); // Configuration struct is now initialized	
	memset(workers, 0, configuration.workers*sizeof(pthread_t));
	memset(SOCKETADDR, 0 , UNIX_MAX_PATH);
	printconf();
	
	
	sprintf(SOCKETADDR, "/tmp/");
	strncat(SOCKETADDR, configuration.sockname, strlen(configuration.sockname));
	puts(SOCKETADDR);
	strncpy(sockaddress.sun_path, SOCKETADDR, UNIX_MAX_PATH);
	
	sockaddress.sun_family = AF_UNIX;
	socket_fd = socket(AF_UNIX, SOCK_STREAM, 0);
	unlink(SOCKETADDR);
	bind(socket_fd, (struct sockaddr *) &sockaddress, sizeof(sockaddress));
	listen(socket_fd, 10);
	while(true){
		com = accept(socket_fd, NULL, 0);
		sleep(2);
		// close(com);
	}
	close(socket_fd);
	free(workers);
	return 0;

}

void* conneciton_handler(void* com){
	int com_fd = (int) com;
}

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