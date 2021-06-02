#include "server.h"
#include "parser.h"
#include "file.h"
#include "client_queue.h"
#include "log.h"
#include "serialization.h"
#define LOG_BUFF 200

 

extern config configuration; // Server config
extern volatile sig_atomic_t can_accept;
extern volatile sig_atomic_t abort_connections;
extern pthread_mutex_t abort_connections_mtx;
extern clients_list *ready_queue[2];
extern clients_list *done_queue[2];

extern bool *free_threads;
extern pthread_mutex_t free_threads_mtx;

extern pthread_mutex_t ready_queue_mtx;
extern pthread_mutex_t done_queue_mtx;
pthread_mutex_t lock_queue_mtx = PTHREAD_MUTEX_INITIALIZER;
extern pthread_cond_t client_is_ready;


extern pthread_mutex_t log_access_mtx;
extern int m_w_pipe[2];

extern void func(clients_list *head);
ssize_t safe_read(int fd, void *ptr, size_t n);
ssize_t safe_write(int fd, void *ptr, size_t n);
int read_all_buffer(int com, unsigned char **buffer, unsigned long *buff_size);
void logger(char *log);


/** TODO:
 * - Port read_all, serialize_response and deserialize_request here to read/write from/to client in one shot
 * - Insert in done_queue broken coms  
*/

static int handle_request(int com, client_request *request, bool *client_waiting_lock){
	int exit_status = -1, lock_com = 0, lock_id = 0;
	char *log_buffer = NULL;
	char *pipe_buffer = NULL;
	server_response response;
	unsigned char* serialized_response = NULL;
	size_t response_size = 0;
	memset(&response, 0, sizeof(response));
	log_buffer = (char *) calloc(LOG_BUFF, sizeof(char));
	if(request->command & OPEN){
		exit_status = open_file(request->pathname, request->command, request->client_id, &response);
		serialize_response(response, &serialized_response, &response_size);
		safe_write(com, serialized_response, response_size);
		reset_buffer(&serialized_response, &response_size);
		// CHECKRW(writen(com, &response, sizeof(response)), sizeof(response), "Writing to client");
	}
	else if(request->command & CLOSE){
		exit_status = close_file(request->pathname, request->client_id, &response); 
		serialize_response(response, &serialized_response, &response_size);
		safe_write(com, serialized_response, response_size);
		reset_buffer(&serialized_response, &response_size);
		// CHECKRW(writen(com, &response, sizeof(response)), sizeof(response), "Writing to client");
	}
	else if(request->command & READ){
		exit_status = read_file(request->pathname, request->client_id, &response);
		serialize_response(response, &serialized_response, &response_size);
		safe_write(com, serialized_response, response_size);
		reset_buffer(&serialized_response, &response_size);
		if(exit_status == 0){
			snprintf(log_buffer, LOG_BUFF, "Client %d read %lu bytes", request->client_id, response.size);
			logger(log_buffer);
		}
	}
	else if(request->command & WRITE){
		exit_status = write_to_file(request->data, request->size, request->pathname, request->client_id, &response);
		serialize_response(response, &serialized_response, &response_size);
		safe_write(com, serialized_response, response_size);
		reset_buffer(&serialized_response, &response_size);
		if(exit_status == 0){
			snprintf(log_buffer, LOG_BUFF, "Client %d wrote %lu bytes", request->client_id, request->size);
			logger(log_buffer);		
		} 
	}
	else if(request->command & APPEND){
		exit_status = append_to_file(request->data, request->size, request->pathname, request->client_id, &response);
		serialize_response(response, &serialized_response, &response_size);
		safe_write(com, serialized_response, response_size);
		reset_buffer(&serialized_response, &response_size);
		if(exit_status == 0){
			snprintf(log_buffer, LOG_BUFF, "Client %d wrote %lu bytes", request->client_id, request->size);
			logger(log_buffer);
		} 
	}
	else if(request->command & SET_LOCK){ // TEST THIS
		if(request->flags & O_LOCK){
			exit_status = lock_file(request->pathname, request->client_id, &response);
			if(exit_status == 0){
				serialize_response(response, &serialized_response, &response_size);
				safe_write(com, serialized_response, response_size);
				reset_buffer(&serialized_response, &response_size);
				snprintf(log_buffer, LOG_BUFF, "Client %d locked %s", request->client_id, request->pathname);
				logger(log_buffer);
			}
			else if(response.code[0] | FILE_LOCKED_BY_OTHERS){
				insert_lock_file_list(request->pathname, request->client_id, com);
				*client_waiting_lock = true;
				snprintf(log_buffer, LOG_BUFF, "Client %d waiting on %s", request->client_id, request->pathname);
				logger(log_buffer);
				free(log_buffer);
				return 0;
			}
			else{
				serialize_response(response, &serialized_response, &response_size);
				safe_write(com, serialized_response, response_size);
				reset_buffer(&serialized_response, &response_size);
				snprintf(log_buffer, LOG_BUFF, "Client %d failed locking %s with error %s", request->client_id, request->pathname, strerror(response.code[1]));
				logger(log_buffer);
			}
		}
		else{
			exit_status = unlock_file(request->pathname, request->client_id, &response);
			if(exit_status == 0){
				serialize_response(response, &serialized_response, &response_size);
				safe_write(com, serialized_response, response_size);
				reset_buffer(&serialized_response, &response_size);
				snprintf(log_buffer, LOG_BUFF, "Client %d unlocked %s", request->client_id, request->pathname);
				logger(log_buffer);
				
				if(pop_lock_file_list(request->pathname, &lock_id, &lock_com) == 0){
					while (fcntl(lock_com, F_GETFD) != 0){
						SAFELOCK(done_queue_mtx);
						insert_client_list(com, &done_queue[0], &done_queue[1]);
						SAFEUNLOCK(done_queue_mtx);
						if(pop_lock_file_list(request->pathname, &lock_id, &lock_com) < 0){
							free(log_buffer);
							return 0;
						}
					}
					memset(&response, 0, sizeof(response));
					lock_file(request->pathname, lock_id, &response); // Add errorcheck per write su log
					serialize_response(response, &serialized_response, &response_size);
					safe_write(com, serialized_response, response_size);
					reset_buffer(&serialized_response, &response_size);
					// CHECKRW(writen(lock_com, &response, sizeof(response)), sizeof(response), "Errore risposta lockfile ");
					pipe_buffer = (char *)calloc(PIPE_BUF, sizeof(char));
					CHECKALLOC(pipe_buffer, "Errore allocazione pipe_buf handler ");
					sprintf(pipe_buffer, "%d", lock_com);
					CHECKRW(writen(m_w_pipe[1], pipe_buffer, PIPE_BUF), PIPE_BUF, "Return com");
					snprintf(log_buffer, LOG_BUFF, "Client %d locked %s", lock_id, request->pathname);
					logger(log_buffer);
					free(pipe_buffer);
				}
				free(log_buffer);
				return 0;
			}
			CHECKRW(writen(com, &response, sizeof(response)), sizeof(response), "Writing to client");
			snprintf(log_buffer, LOG_BUFF, "Client %d failed locking %s with error %s", request->client_id, request->pathname, strerror(response.code[1]));
			logger(log_buffer);
			free(log_buffer);
			return -1;
		} 
	}
	free(log_buffer);
	return exit_status;
}

void* worker(void* args){
	int com = 0;
	size_t request_buffer_size = 0;
	bool client_waiting_lock = false;
	int whoami = *(int*) args;
	char buffer[PIPE_BUF];
	char log_buffer[200];
	int request_status = 0;
	unsigned char* request_buffer;
	client_request request;
	
	memset(buffer, 0, PIPE_BUF);
	memset(log_buffer, 0, 200);
	memset(&request, 0, sizeof(request));
	

	while(true){
		fflush(stdout);

		// Thread waits for work to be assigned
		SAFELOCK(ready_queue_mtx);
		while(ready_queue[1] == NULL){
			SAFELOCK(abort_connections_mtx);
			if(abort_connections){
				SAFEUNLOCK(abort_connections_mtx);
				SAFEUNLOCK(ready_queue_mtx);
				puts(ANSI_COLOR_RED"EXIT 0"ANSI_COLOR_RESET);
				return (void *) 0;
			}
			SAFEUNLOCK(abort_connections_mtx);
			pthread_cond_wait(&client_is_ready, &ready_queue_mtx); 
		}
		SAFELOCK(free_threads_mtx);
		free_threads[whoami] = false;
		SAFEUNLOCK(free_threads_mtx);
		com = pop_client(&ready_queue[0], &ready_queue[1]); // Pop dalla lista dei socket ready che va fatta durante il lock		
		SAFEUNLOCK(ready_queue_mtx);
		if(com == -1) // Falso allarme
			continue;
		printf(ANSI_COLOR_MAGENTA"[Thread %d] received request from client %d\n"ANSI_COLOR_RESET, whoami, com);
		// snprintf(log_buffer, LOG_BUFF, "[Thread %d] received request from client %d", whoami, com);
		// logger(log_buffer);;
		
		client_waiting_lock = false;
		read_all_buffer(com, &request_buffer, &request_buffer_size);
		deserialize_request(&request, &request_buffer, request_buffer_size);
		if(request.command & QUIT) {
			memset(buffer, 0, sizeof(buffer));
			SAFELOCK(free_threads_mtx);
			free_threads[whoami] = true;
			SAFEUNLOCK(free_threads_mtx);
			SAFELOCK(done_queue_mtx);
			insert_client_list(com, &done_queue[0], &done_queue[1]);
			SAFEUNLOCK(done_queue_mtx);
			
			continue;
		}
		request_status = handle_request(com, &request, &client_waiting_lock); // Response is set and log is updated

		if(request_status < 0){
			sprintf(log_buffer,"[Thread %d] Error handling client %d request", whoami, request.client_id);
			logger(log_buffer);
			
			continue;
		}
		
		if(!client_waiting_lock){ // Se il client aspetta la lock non lo rimando in polling
			memset(buffer, 0, sizeof(buffer));
			sprintf(buffer, "%d", com);
			CHECKRW(writen(m_w_pipe[1], buffer, sizeof(buffer)), sizeof(buffer), "Return com");
		}	
		SAFELOCK(free_threads_mtx);
		free_threads[whoami] = true;
		SAFEUNLOCK(free_threads_mtx);
		SAFELOCK(abort_connections_mtx);
		if(abort_connections){
			SAFEUNLOCK(abort_connections_mtx);
			break;
		}
		SAFEUNLOCK(abort_connections_mtx);
	}
	puts(ANSI_COLOR_RED"EXIT"ANSI_COLOR_RESET);
	return (void *) 0;
}

ssize_t safe_write(int fd, void *ptr, size_t n){
	int exit_status = 0;
	if((exit_status = writen(fd, ptr, n)) < 0){
		SAFELOCK(done_queue_mtx);
		insert_client_list(fd, &done_queue[0], &done_queue[1]);
		SAFEUNLOCK(done_queue_mtx);
		return -1;
	}
	return exit_status;
}

ssize_t safe_read(int fd, void *ptr, size_t n){
	int exit_status = 0;
	if((exit_status = readn(fd, ptr, n)) < 0){
		SAFELOCK(done_queue_mtx);
		insert_client_list(fd, &done_queue[0], &done_queue[1]);
		SAFEUNLOCK(done_queue_mtx);
		return -1;
	}
	return exit_status;
}

int read_all_buffer(int com, unsigned char **buffer, unsigned long *buff_size){
	int index = 0, nreads = 0, read_bytes = 0;
	*buff_size = 1024;
	*buffer = realloc(*buffer, *buff_size);
	memset(*buffer, 0, sizeof(unsigned char));
	do{
		read_bytes = safe_read(com, *buffer + index, *buff_size - index);
		if(read_bytes < 0) return -1;
		nreads += read_bytes;
		if(nreads >= *buff_size){
			*buff_size += 1024;
			*buffer = realloc(*buffer, *buff_size);
			CHECKALLOC(*buffer, "Erorre di riallocazione durante la read dal socket");
		}
		index += read_bytes;
	}while(read_bytes > 0);
	return 0;
}

void logger(char *log){
	SAFELOCK(log_access_mtx);
	write_to_log(log);
	SAFEUNLOCK(log_access_mtx);
}
