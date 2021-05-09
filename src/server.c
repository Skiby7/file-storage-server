#include "server.h"
#include "parser.h"
#define _GNU_SOURCE 

config configuration; // Server config
volatile sig_atomic_t can_accept = true;
static pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER;


int main(int argc, char* argv[]){ // REMEMBER FFLUSH FOR THREAD PRINTF


	
	char SOCKETADDR[UNIX_MAX_PATH];
	struct sockaddr_un sockaddress;
	pargs targs;
	int socket_fd = 0, com = 0, i = 0; 
	unsigned int seed = time(NULL);
	struct sigaction sig;
	char buff[10];
	memset(&sig, 0, sizeof(sig));
	sig.sa_handler=signal_handler;
	sigaction(SIGINT,&sig,NULL);
	sigaction(SIGHUP,&sig,NULL);
	sigaction(SIGQUIT,&sig,NULL);
	



	init(SOCKETADDR); // Configuration struct is now initialized
	pthread_t *workers = (pthread_t *) malloc(configuration.workers*sizeof(pthread_t));
	memset(workers, 0, configuration.workers*sizeof(pthread_t));
	memset(&targs, 0, sizeof(pargs));
	memset(buff, 0, 10);
	printconf();
	puts(SOCKETADDR);
	strncpy(sockaddress.sun_path, SOCKETADDR, UNIX_MAX_PATH);
	fflush(stdout);
	sockaddress.sun_family = AF_UNIX;
	socket_fd = socket(AF_UNIX, SOCK_STREAM, 0);
	unlink(SOCKETADDR);
	bind(socket_fd, (struct sockaddr *) &sockaddress, sizeof(sockaddress));
	listen(socket_fd, 10);
	while(true){

		com = accept(socket_fd, NULL, 0);
		if(can_accept){
			sprintf(buff, "accepted");
			write(com, buff, strlen(buff));
			memset(buff, 0, sizeof(buff));
			i = rand_r(&seed)%configuration.workers;
			targs.socket_fd = com;
			targs.whoami = i;
			pthread_create(&workers[i], NULL, conneciton_handler, (void *) &targs);
			// pthread_detach(workers[i]);
			// memset(&targs, 0, sizeof(targs));
		}
		else{
			sprintf(buff, "refused");
			write(com, buff, strlen(buff));
			memset(buff, 0, sizeof(buff));
			close(com);

			
		}
		
	}
	close(socket_fd);
	free(workers);
	return 0;

}

void* conneciton_handler(void *args){
	int com, read_bytes;
	short whoami;
	char buff[100];
	memset(buff, 0, 100);
	pthread_mutex_lock(&mtx);
	pargs *targs = args;
	com = targs->socket_fd;
	whoami = targs->whoami;
	pthread_mutex_unlock(&mtx);
	printf(ANSI_COLOR_MAGENTA"Thread %d ready on client %d\n"ANSI_COLOR_RESET, whoami, com);
	while (1){
		if((read_bytes = read(com, buff, 99)) > 0){
			if(strcmp(buff, "quit") == 0){
				printf(ANSI_COLOR_YELLOW"Exiting from thread %d\n"ANSI_COLOR_RESET, whoami);
				close(com);
				return (void *) 0;
			}
			// fflush(stdout);
			printf("Read %d bytes -> %s\n", read_bytes, buff);
			// fflush(stdout);
			memset(buff, 0, 100);
			sprintf(buff, "Read %d bytes", read_bytes);
			write(com, buff, strlen(buff));
			memset(buff, 0, 100);
		}
	}
	close(com);
}

static void signal_handler(int signum){
	if(signum == SIGINT){
		puts(ANSI_COLOR_RED"Recived SIGINT"ANSI_COLOR_RESET);
		can_accept = false;

		// exit(EXIT_SUCCESS);
	}
	if(signum == SIGQUIT){
		puts(ANSI_COLOR_RED"Recived SIGQUIT"ANSI_COLOR_RESET);
		exit(EXIT_SUCCESS);
	}
	if(signum == SIGHUP){
		puts(ANSI_COLOR_RED"Recived SIGHUP"ANSI_COLOR_RESET);
		exit(EXIT_SUCCESS);
	}

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