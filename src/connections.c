#include "server.h"
#include "parser.h"

extern config configuration; // Server config
extern volatile sig_atomic_t can_accept;
extern pthread_mutex_t mtx;
extern unsigned int *active_connecitons;

void* conneciton_handler(void *args){
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

void* refuse_connection(void* args){
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