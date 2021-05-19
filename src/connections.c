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
		pthread_mutex_lock(&ready_queue_mtx);
		while(ready_queue[1] == NULL){
			pthread_mutex_lock(&abort_connections_mtx);
			if(abort_connections){
				pthread_mutex_unlock(&abort_connections_mtx);
				pthread_mutex_unlock(&ready_queue_mtx);
				puts(ANSI_COLOR_RED"EXIT 0"ANSI_COLOR_RESET);
				return (void *) 0;
			}
			pthread_mutex_unlock(&abort_connections_mtx);
			pthread_cond_wait(&client_is_ready, &ready_queue_mtx); 
		}
		pthread_mutex_lock(&free_threads_mtx);
		free_threads[whoami] = false;
		pthread_mutex_unlock(&free_threads_mtx);
		com = pop_client(&ready_queue[0], &ready_queue[1]); // Pop dalla lista dei socket ready che va fatta durante il lock		
		pthread_mutex_unlock(&ready_queue_mtx);
		if(com == -1) // Falso allarme
			continue;
		printf(ANSI_COLOR_MAGENTA"[Thread %d] received request from client %d\n"ANSI_COLOR_RESET, whoami, com);
		sprintf(log_buffer,"[Thread %d] received request from client %d", whoami, com);
		pthread_mutex_lock(&log_access_mtx);
		write_to_log(log_buffer);
		pthread_mutex_unlock(&log_access_mtx);
		
	
		CHECKERRNO((read(com, buffer, sizeof(buffer)) < 0), "Reading from client");
		if(strcmp(buffer, "quit") == 0) {
			close(com);
			memset(buffer, 0, sizeof(buffer));
			pthread_mutex_lock(&free_threads_mtx);
			free_threads[whoami] = true;
			pthread_mutex_unlock(&free_threads_mtx);
			printf(ANSI_COLOR_MAGENTA"[Thread %d] client %d quitted, com closed\n"ANSI_COLOR_RESET, whoami, com);
			sprintf(log_buffer,"[Thread %d] client %d quitted, com closed", whoami, com);
			pthread_mutex_lock(&log_access_mtx);
			write_to_log(log_buffer);
			pthread_mutex_unlock(&log_access_mtx);
			continue;
		}	
		CHECKERRNO((write(com, accepted, strlen(accepted)) < 0), "Writing to client");
		
		printf(ANSI_COLOR_MAGENTA"[Thread %d - client %d]:"ANSI_COLOR_CYAN" %s\n"ANSI_COLOR_RESET, whoami, com, buffer);
		sprintf(log_buffer,"[Thread %d - client %d]: ", whoami, com);
		strncat(log_buffer, buffer, 150); // buffer has a size of PIPE_BUF (4096 bytes) 
		pthread_mutex_lock(&log_access_mtx);
		write_to_log(log_buffer);
		pthread_mutex_unlock(&log_access_mtx);
		memset(buffer, 0, sizeof(buffer));
		sprintf(buffer, "%d", com);
		CHECKERRNO((write(m_w_pipe[1], buffer, sizeof(buffer)) < 0), "Return com");
		memset(buffer, 0, sizeof(buffer));	
		pthread_mutex_lock(&free_threads_mtx);
		free_threads[whoami] = true;
		pthread_mutex_unlock(&free_threads_mtx);
		pthread_mutex_lock(&abort_connections_mtx);
		if(abort_connections){
			pthread_mutex_unlock(&abort_connections_mtx);
			break;
		}
		pthread_mutex_unlock(&abort_connections_mtx);
	}
	puts(ANSI_COLOR_RED"EXIT"ANSI_COLOR_RESET);
	return (void *) 0;
}




