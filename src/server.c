#include "server.h"
#include "parser.h"
#define _GNU_SOURCE 

config configuration; // Server config
volatile sig_atomic_t can_accept = true;
static pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER;
unsigned int *active_connecitons;



int main(int argc, char* argv[]){ // REMEMBER FFLUSH FOR THREAD PRINTF
	
	int socket_fd = 0, com = 0, i = 0; 
	char buff[10];
	char SOCKETADDR[UNIX_MAX_PATH];
	pthread_t waiter;
	pthread_t refuser;
	struct sockaddr_un sockaddress;
	pargs targs;
	unsigned int seed = time(NULL);
	struct sigaction sig;
	memset(&sig, 0, sizeof(sig));
	sig.sa_handler=signal_handler;
	sigaction(SIGINT,&sig,NULL);
	sigaction(SIGHUP,&sig,NULL);
	sigaction(SIGQUIT,&sig,NULL);
	



	init(SOCKETADDR); // Configuration struct is now initialized
	pthread_t *workers = (pthread_t *) malloc(configuration.workers*sizeof(pthread_t));
	active_connecitons = (unsigned int *) malloc(configuration.workers*sizeof(unsigned int));
	memset(workers, 0, configuration.workers*sizeof(pthread_t));
	memset(active_connecitons, 0, configuration.workers*sizeof(int));
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

		}
		else{
			pthread_create(&waiter, NULL, wait_workers, &workers);
			pthread_create(&refuser, NULL, refuse_connection, &socket_fd);
	pthread_detach(refuser);
			
			puts(ANSI_COLOR_BLUE"Sono uscito dal dispatcher"ANSI_COLOR_RESET);
			break;
			
		}
		
	}
	pthread_join(waiter, NULL);
	puts(ANSI_COLOR_GREEN"Joined waiter"ANSI_COLOR_RESET);
	close(socket_fd);
	free(workers);
	return 0;

}

static void* conneciton_handler(void *args){
	int com, read_bytes;
	short whoami;
	char buff[100];
	memset(buff, 0, 100);
	pthread_mutex_lock(&mtx);
	pargs *targs = args;
	com = targs->socket_fd;
	whoami = targs->whoami;
	active_connecitons[whoami]++;
	pthread_mutex_unlock(&mtx);
	printf(ANSI_COLOR_MAGENTA"Thread %d ready on client %d\n"ANSI_COLOR_RESET, whoami, com);
	while (true){
		if((read_bytes = read(com, buff, 99)) > 0){
			if(strcmp(buff, "quit") == 0){
				printf(ANSI_COLOR_YELLOW"Exiting from thread %d\n"ANSI_COLOR_RESET, whoami);
				close(com);
				pthread_mutex_lock(&mtx);
				active_connecitons[whoami]--;
				pthread_mutex_unlock(&mtx);
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

static void* refuse_connection(void* args){
	int com, socket_fd;
	socket_fd = *((int*) args);
	char buff[] = "refused";
	while(true){
		com = accept(socket_fd, NULL, 0);
		write(com, buff, strlen(buff));
		close(com);	
	}
	return (void *) 0;

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



void* wait_workers(void* args){
	pthread_t *workers = *((pthread_t**)args);
	int i = 0;
	while(true){
		pthread_mutex_lock(&mtx);
		while(i < configuration.workers && active_connecitons[i] == 0) {i++;}
		pthread_mutex_unlock(&mtx);
		if(i != configuration.workers)
			printf("Waiting thread %d\n", i);
		if(i == configuration.workers)
			break;
		else{
			puts("Before join wait workers");
			pthread_join(workers[i], NULL);
			puts("After join wait workers");
		}
		i = 0;
	}
	
	return (void *) 0;
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