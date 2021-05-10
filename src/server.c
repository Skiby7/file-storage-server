#include "server.h"
#include "parser.h"
#include "file.h"
#define _GNU_SOURCE 

config configuration; // Server config
volatile sig_atomic_t can_accept = true;
volatile sig_atomic_t abort_connections = false;
pthread_mutex_t targs_mtx = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t targs_read_cond = PTHREAD_COND_INITIALIZER;
bool targs_read = false;

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
	return 0;
	strncpy(sockaddress.sun_path, SOCKETADDR, UNIX_MAX_PATH);
	fflush(stdout);
	sockaddress.sun_family = AF_UNIX;
	socket_fd = socket(AF_UNIX, SOCK_STREAM, 0);
	unlink(SOCKETADDR);
	bind(socket_fd, (struct sockaddr *) &sockaddress, sizeof(sockaddress));
	listen(socket_fd, 10);
	while(true){

		if(can_accept){
			com = accept(socket_fd, NULL, 0);
			sprintf(buff, "accepted");
			write(com, buff, strlen(buff));
			memset(buff, 0, sizeof(buff));
			i = rand_r(&seed)%configuration.workers;
			targs.socket_fd = com;
			targs.whoami = i;
			pthread_create(&workers[i], NULL, connection_handler, (void *) &targs);
			sleep(2);
			pthread_mutex_lock(&targs_mtx);
			while(!targs_read) pthread_cond_wait(&targs_read_cond, &targs_mtx);
			targs_read = false;
			pthread_mutex_unlock(&targs_mtx);
			
		}
		else{
			
			pthread_create(&waiter, NULL, wait_workers, &workers);
			pthread_create(&refuser, NULL, refuse_connection, &socket_fd);
			
			pthread_detach(refuser);
			
			puts(ANSI_COLOR_BLUE"Tutti i client si sono disconnesi e sono uscito dal dispatcher"ANSI_COLOR_RESET);
						
			break;
			
		}
		
	}
	pthread_join(waiter, NULL);
	close(socket_fd);
	free(workers);
	return 0;

}





void signal_handler(int signum){
	
	if(signum == SIGHUP){
		puts(ANSI_COLOR_RED"Recived SIGHUP"ANSI_COLOR_RESET);
		can_accept = false;
	}

	else{
		if(signum == SIGQUIT)
			puts(ANSI_COLOR_RED"Recived SIGQUIT"ANSI_COLOR_RESET);
		if(signum == SIGINT)
			puts(ANSI_COLOR_RED"Recived SIGINT"ANSI_COLOR_RESET);
		abort_connections = true;
		can_accept = false;
	}

}




	
	

void printconf(){
	printf("Workers: %d\n"
			"Mem: %d\n"
			"Files: %d\n"
			"Sockname: %s\n"
			"Log: %s\n", 
			configuration.workers, configuration.mem, 
			configuration.files, configuration.sockname, configuration.log);
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