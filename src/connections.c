#include "server.h"
#include "parser.h"
#include "file.h"

extern config configuration; // Server config
extern volatile sig_atomic_t can_accept;
extern volatile sig_atomic_t abort_connections;
extern pthread_mutex_t targs_mtx;
extern unsigned int *active_connecitons;
extern pthread_cond_t targs_read_cond;
extern bool targs_read;


static pthread_mutex_t active_connections_mutex = PTHREAD_MUTEX_INITIALIZER;

static connection *active_coms = NULL;

void* connection_handler(void *args){
	
	pthread_mutex_lock(&targs_mtx);
	pargs *targs = args;
	int com = targs->socket_fd;
	short whoami = targs->whoami;
	targs_read = true;
	pthread_cond_signal(&targs_read_cond);
	pthread_mutex_unlock(&targs_mtx);

	pthread_mutex_lock(&active_connections_mutex);
	active_connecitons[whoami]++;
	pthread_mutex_unlock(&active_connections_mutex);

	int read_bytes;
	char buff[100];
	memset(buff, 0, 100);
	printf(ANSI_COLOR_MAGENTA"Thread %d ready on client %d\n"ANSI_COLOR_RESET, whoami, com);
	// active_coms = insert_com(active_coms, com);
	while (true){
		if(abort_connections){
				close(com);
				printf(ANSI_COLOR_CYAN"Closed connection with client %d\n"ANSI_COLOR_RESET, whoami);
				return (void *)0;
			}
		if((read_bytes = read(com, buff, 99)) > 0){
			if(strcmp(buff, "quit") == 0){
				printf(ANSI_COLOR_YELLOW"Exiting from thread %d\n"ANSI_COLOR_RESET, whoami);

				close(com);
				pthread_mutex_lock(&targs_mtx);
				active_connecitons[whoami]--;
				pthread_mutex_unlock(&targs_mtx);
				return (void *) 0;
			}
			
			printf("Read %d bytes -> %s\n", read_bytes, buff);
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
		pthread_mutex_lock(&active_connections_mutex);
		while(i < configuration.workers && active_connecitons[i] == 0) {i++;}
		pthread_mutex_unlock(&active_connections_mutex);
		puts("ho controllato le connessioni");
		if(i != configuration.workers)
			printf("Waiting thread %d\n", i);
		if(i == configuration.workers)
			break;
		else
			pthread_join(workers[i], NULL);
		i = 0;
	}
	
	return (void *) 0;
}

// static connection* insert_com(connection *head, int com)
// {
//     connection *new = (connection *)malloc(sizeof(connection));
//     new->com = com;
// 	new->next = head;
    
//     return new;
// }

// static connection* remove_com(connection *head, int com)
// {
// 	connection *new = head;
// 	connection *select = NULL;
//     while(head->com != com && head != NULL){
// 		select = head;
// 		head = head->next;
// 	}

		
    
    
// }