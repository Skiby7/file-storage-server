#include "server.h"
#include "parser.h"
#include "file.h"
#include "client_queue.h"
#include "log.h"

extern config configuration; // Server config
extern volatile sig_atomic_t can_accept;
extern volatile sig_atomic_t abort_connections;
extern pthread_mutex_t abort_connections_mtx;
extern ready_clients *ready_queue[2];
extern bool *free_threads;
extern pthread_mutex_t free_threads_mtx;

extern pthread_mutex_t ready_queue_mtx;
extern pthread_cond_t client_is_ready;


extern pthread_mutex_t log_access_mtx;
extern int m_w_pipe[2];

extern void func(ready_clients *head);
// static connection *active_coms = NULL;

// void* connection_handler(void *args){
	
// 	pthread_mutex_lock(&targs_mtx);
// 	pargs *targs = args;
// 	int com = targs->socket_fd;
// 	short whoami = targs->whoami;
// 	targs_read = true;
// 	pthread_cond_signal(&targs_read_cond);
// 	pthread_mutex_unlock(&targs_mtx);

// 	pthread_mutex_lock(&active_connections_mutex);
// 	active_connecitons[whoami]++;
// 	pthread_mutex_unlock(&active_connections_mutex);

// 	int read_bytes;
// 	char buff[100];
// 	memset(buff, 0, 100);
// 	printf(ANSI_COLOR_MAGENTA"Thread %d ready on client %d\n"ANSI_COLOR_RESET, whoami, com);
// 	// active_coms = insert_com(active_coms, com);
// 	while (true){
// 		if(abort_connections){
// 				close(com);
// 				printf(ANSI_COLOR_CYAN"Closed connection with client %d\n"ANSI_COLOR_RESET, whoami);
// 				return (void *)0;
// 			}
// 		if((read_bytes = read(com, buff, 99)) > 0){
// 			if(strcmp(buff, "quit") == 0){
// 				printf(ANSI_COLOR_YELLOW"Exiting from thread %d\n"ANSI_COLOR_RESET, whoami);

// 				close(com);
// 				pthread_mutex_lock(&targs_mtx);
// 				active_connecitons[whoami]--;
// 				pthread_mutex_unlock(&targs_mtx);
// 				return (void *) 0;
// 			}
			
// 			printf("Read %d bytes -> %s\n", read_bytes, buff);
// 			memset(buff, 0, 100);
// 			sprintf(buff, "Read %d bytes", read_bytes);
// 			write(com, buff, strlen(buff));
// 			memset(buff, 0, 100);
// 		}
// 	}
// 	close(com);
// }

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

// void* wait_workers(void* args){
// 	pthread_t *workers = *((pthread_t**)args);
// 	int i = 0;
// 	while(true){
// 		pthread_mutex_lock(&active_connections_mutex);
// 		while(i < configuration.workers && active_connecitons[i] == 0) {i++;}
// 		pthread_mutex_unlock(&active_connections_mutex);
// 		puts("ho controllato le connessioni");
// 		if(i != configuration.workers)
// 			printf("Waiting thread %d\n", i);
// 		if(i == configuration.workers)
// 			break;
// 		else
// 			pthread_join(workers[i], NULL);
// 		i = 0;
// 	}
	
// 	return (void *) 0;
// }


void* worker(void* args){
	int com = 0;
	int whoami = *(int*) args;
	char buffer[PIPE_BUF];
	char accepted[] = "accepted";
	char log_buffer[200];
	memset(buffer, 0, PIPE_BUF);
	memset(log_buffer, 0, 200);

	fflush(stdout);
	while(true){
		// Thread waits for work to be assigned
		// printf(ANSI_COLOR_RED"Thread %d -> queue mutex lock\n"ANSI_COLOR_RESET, whoami);
		// puts("qui");
		pthread_mutex_lock(&ready_queue_mtx);
		// puts("ma non qui");
		while(ready_queue[1] == NULL){
			// printf(ANSI_COLOR_RED"Thread %d -> wait signal\n"ANSI_COLOR_RESET, whoami);
			pthread_cond_wait(&client_is_ready, &ready_queue_mtx); // NULL -> placeholder
			// printf(ANSI_COLOR_RED"Thread %d -> stopped wait signal\n"ANSI_COLOR_RESET, whoami);
		}
		pthread_mutex_lock(&free_threads_mtx);
		free_threads[whoami] = false;
		pthread_mutex_unlock(&free_threads_mtx);
		com = pop_client(&ready_queue[0], &ready_queue[1]); // Pop dalla lista dei socket ready che va fatta durante il lock
		
		pthread_mutex_unlock(&ready_queue_mtx);
		// printf(ANSI_COLOR_RED"Thread %d -> queue mutex unlock\n"ANSI_COLOR_RESET, whoami);
		printf(ANSI_COLOR_MAGENTA"[Thread %d] received request from client %d\n"ANSI_COLOR_RESET, whoami, com);
		sprintf(log_buffer,"[Thread %d] received request from client %d\n", whoami, com);
		pthread_mutex_lock(&log_access_mtx);
		write_to_log(log_buffer);
		pthread_mutex_unlock(&log_access_mtx);
		
		// puts("provo a fare la write");
		// CHECKERRNO((write(com, accepted, strlen(accepted) ) < 0), "Writing to client");
		// puts("e crasho");
		CHECKERRNO((read(com, buffer, sizeof(buffer)) < 0), "Reading from client");
		if(strcmp(buffer, "quit") == 0) {
			close(com);
			memset(buffer, 0, sizeof(buffer));
			pthread_mutex_lock(&free_threads_mtx);
			free_threads[whoami] = true;
			pthread_mutex_unlock(&free_threads_mtx);
			printf(ANSI_COLOR_MAGENTA"[Thread %d] client %d quitted, com closed\n"ANSI_COLOR_RESET, whoami, com);
			continue;
		}	
		CHECKERRNO((write(com, accepted, strlen(accepted)) < 0), "Writing to client");
		
		printf(ANSI_COLOR_MAGENTA"[Thread %d - client %d]:"ANSI_COLOR_CYAN" %s\n"ANSI_COLOR_RESET, whoami, com, buffer);
		memset(buffer, 0, sizeof(buffer));
		sprintf(buffer, "%d", com);
		CHECKERRNO((write(m_w_pipe[1], buffer, sizeof(buffer)) < 0), "Return com");
		memset(buffer, 0, sizeof(buffer));	
		pthread_mutex_lock(&free_threads_mtx);
		free_threads[whoami] = true;
		pthread_mutex_unlock(&free_threads_mtx);
	}
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