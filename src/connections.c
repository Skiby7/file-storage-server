#include "server.h"
#include "parser.h"
#include "file.h"
#include "client_queue.h"
#include "log.h"
#define LOG_BUFF 200

 

extern config configuration; // Server config
extern volatile sig_atomic_t can_accept;
extern volatile sig_atomic_t abort_connections;
extern pthread_mutex_t abort_connections_mtx;
extern clients_list *ready_queue[2];
extern clients_list *done_queue[2];
extern lock_waiters_list *lock_waiters_queue[2];

extern bool *free_threads;
extern pthread_mutex_t free_threads_mtx;

extern pthread_mutex_t ready_queue_mtx;
extern pthread_mutex_t done_queue_mtx;
pthread_mutex_t lock_queue_mtx = PTHREAD_MUTEX_INITIALIZER;
extern pthread_cond_t client_is_ready;


extern pthread_mutex_t log_access_mtx;
extern int m_w_pipe[2];

extern void func(clients_list *head);
ssize_t readn(int fd, void *ptr, size_t n);
ssize_t writen(int fd, void *ptr, size_t n);

/** TODO:
 * - Implementare una lista globale di lock waiters con associato com e client id
 * - Implementare close_file server side
*/





static int handle_request(int com, client_request *request, bool *client_waiting_lock){
	int exit_status = -1, lock_com = 0, lock_id = 0;
	char *log_buffer = NULL;
	char *data_buffer = NULL;
	char *pipe_buffer = NULL;
	server_response response;
	memset(&response, 0, sizeof(response));
	log_buffer = (char *) calloc(LOG_BUFF, sizeof(char));
	if(request->command & OPEN){
		exit_status = open_file(request->pathname, request->command, request->client_id, &response);
		CHECKRW(writen(com, &response, sizeof(response)), sizeof(response), "Writing to client");
	}
	else if(request->command & CLOSE){
		exit_status = close_file(request->pathname, request->client_id, &response); 
		CHECKRW(writen(com, &response, sizeof(response)), sizeof(response), "Writing to client");
	}
	else if(request->command & READ){
		exit_status = read_file(request->pathname, &data_buffer, request->client_id, &response);
		if(exit_status == 0){
			CHECKRW(writen(com, &response, sizeof(response)), sizeof(response), "Writing to client");
			CHECKRW(readn(com, request, sizeof(client_request)), sizeof(client_request), "Writing to client");
			if(request->command & FILE_OPERATION_SUCCESS){
				CHECKRW(writen(com, data_buffer, response.size), response.size, "Writing to client");
				sprintf(log_buffer,"Client %d read %d bytes", request->client_id, response.size);
				SAFELOCK(log_access_mtx);
				write_to_log(log_buffer);
				SAFEUNLOCK(log_access_mtx);
				free(data_buffer);
			}
			else{
				free(data_buffer);
				return -1;
			} 
		}
	}
	else if(request->command & WRITE){
		
		response.code[0] = FILE_OPERATION_SUCCESS;
		data_buffer = (unsigned char *)calloc(request->size, sizeof(unsigned char));
		CHECKRW(writen(com, &response, sizeof(response)), sizeof(response), "Writing to client");
		CHECKRW(readn(com, data_buffer, request->size), request->size, "Reading from client");

		exit_status = write_to_file(data_buffer, request->size, request->pathname, request->client_id, &response);
		
		if(exit_status == 0){
			CHECKRW(writen(com, &response, sizeof(response)), sizeof(response), "Writing to client");
			sprintf(log_buffer,"Client %d wrote %d bytes", request->client_id, request->size);
			SAFELOCK(log_access_mtx);
			write_to_log(log_buffer);
			SAFEUNLOCK(log_access_mtx);			
			free(data_buffer);
		}
		else{
			free(data_buffer);
			return -1;
		} 

	}
	else if(request->command & APPEND){
		response.code[0] = FILE_OPERATION_SUCCESS;
		CHECKRW(writen(com, &response, sizeof(response)), sizeof(response), "Writing to client");
		CHECKRW(readn(com, data_buffer, request->size), request->size, "Reading from client");
		exit_status = append_to_file(data_buffer, request->size, request->pathname, request->client_id, &response);
		if(exit_status == 0){
			CHECKRW(write(com, data_buffer, response.size),  response.size, "Writing to client");
			sprintf(log_buffer,"Client %d wrote %d bytes", request->client_id, request->size);
			SAFELOCK(log_access_mtx);
			write_to_log(log_buffer);
			SAFEUNLOCK(log_access_mtx);
			free(data_buffer);
		}
		else{
			free(data_buffer);
			return -1;
		} 
	}
	else if(request->command & SET_LOCK){ // TEST THIS
		if(request->flags & O_LOCK){
			exit_status = lock_file(request->pathname, request->client_id, &response);
			if(exit_status == 0){
				CHECKRW(writen(com, &response, sizeof(response)), sizeof(response), "Writing to client");
				sprintf(log_buffer,"Client %d locked %s", request->client_id, request->pathname);
				SAFELOCK(log_access_mtx);
				write_to_log(log_buffer);
				SAFEUNLOCK(log_access_mtx);
			}
			else if(response.code[0] | FILE_LOCKED_BY_OTHERS){
				SAFELOCK(lock_queue_mtx);
				insert_lock_list(com, request->client_id, request->pathname, &lock_waiters_queue[0], &lock_waiters_queue[1]);
				SAFEUNLOCK(lock_queue_mtx);
				*client_waiting_lock = true;
				sprintf(log_buffer,"Client %d waiting on %s", request->client_id, request->pathname);
				SAFELOCK(log_access_mtx);
				write_to_log(log_buffer);
				SAFEUNLOCK(log_access_mtx);
				free(log_buffer);
				return 0;
			}
			else{
				CHECKRW(writen(com, &response, sizeof(response)), sizeof(response), "Writing to client");
				sprintf(log_buffer,"Client %d failed locking %s with error %s", request->client_id, request->pathname, strerror(response.code[1]));
				SAFELOCK(log_access_mtx);
				write_to_log(log_buffer);
				SAFEUNLOCK(log_access_mtx);
				free(log_buffer);
				return -1;
			}
		}
		else{
			exit_status = unlock_file(request->pathname, request->client_id, &response);
			if(exit_status == 0){
				CHECKRW(writen(com, &response, sizeof(response)), sizeof(response), "Writing to client");
				sprintf(log_buffer,"Client %d locked %s", request->client_id, request->pathname);
				SAFELOCK(log_access_mtx);
				write_to_log(log_buffer);
				SAFEUNLOCK(log_access_mtx);
				SAFELOCK(lock_queue_mtx);
				lock_com = get_lock_client(&lock_id, request->pathname, &lock_waiters_queue[0], &lock_waiters_queue[1]);
				SAFEUNLOCK(lock_queue_mtx);
				if(lock_com > 0){
					pipe_buffer = (char *)calloc(PIPE_BUF, sizeof(char));
					CHECKALLOC(pipe_buffer, "Errore allocazione pipe_buf handler ");
					while (fcntl(lock_com, F_GETFD) != 0){
						sprintf(pipe_buffer, "%d", lock_com);
						CHECKRW(writen(m_w_pipe[1], pipe_buffer, PIPE_BUF), PIPE_BUF, "Return com");

						SAFELOCK(lock_queue_mtx);
						lock_com = get_lock_client(&lock_id, request->pathname, &lock_waiters_queue[0], &lock_waiters_queue[1]);
						SAFEUNLOCK(lock_queue_mtx);
						if(lock_com < 0) return 0;
					}
					memset(&response, 0, sizeof(response));
					lock_file(request->pathname, lock_id, &response); // Add errorcheck per write su log
					CHECKRW(writen(lock_com, &response, sizeof(response)), sizeof(response), "Errore risposta lockfile ");
					sprintf(log_buffer,"Client %d locked %s", lock_id, request->pathname);
					SAFELOCK(log_access_mtx);
					write_to_log(log_buffer);
					SAFEUNLOCK(log_access_mtx);
					free(pipe_buffer);
					free(log_buffer);
					return 0;
				}
				free(log_buffer);
				return 0;
			}
			CHECKRW(writen(com, &response, sizeof(response)), sizeof(response), "Writing to client");
			sprintf(log_buffer,"Client %d failed locking %s with error %s", request->client_id, request->pathname, strerror(response.code[1]));
			SAFELOCK(log_access_mtx);
			write_to_log(log_buffer);
			SAFEUNLOCK(log_access_mtx);
			free(log_buffer);
			return -1;
			
			
		} 
	}
	free(log_buffer);
	return exit_status;
}




void* worker(void* args){
	int com = 0;
	bool client_waiting_lock = false;
	int whoami = *(int*) args;
	char buffer[PIPE_BUF];
	char log_buffer[200];
	int request_status = 0;
	client_request request;
	server_response respond_to_client;
	memset(buffer, 0, PIPE_BUF);
	memset(log_buffer, 0, 200);
	memset(&request, 0, sizeof(request));
	memset(&respond_to_client, 0, sizeof(respond_to_client));
	

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
		// sprintf(log_buffer,"[Thread %d] received request from client %d", whoami, com);
		// SAFELOCK(log_access_mtx);
		// write_to_log(log_buffer);
		// SAFEUNLOCK(log_access_mtx);
		
		client_waiting_lock = false;
		CHECKRW(readn(com, &request, sizeof(request)), sizeof(request), "Reading from client");
		if(request.command & QUIT) {
			memset(buffer, 0, sizeof(buffer));
			SAFELOCK(free_threads_mtx);
			free_threads[whoami] = true;
			SAFEUNLOCK(free_threads_mtx);
			SAFELOCK(done_queue_mtx);
			insert_client_list(com, &done_queue[0], &done_queue[1]);
			SAFEUNLOCK(done_queue_mtx);
			// memset(buffer, 0, sizeof(buffer));	
			// printf(ANSI_COLOR_MAGENTA"[Thread %d] client %d quitted, com closed\n"ANSI_COLOR_RESET, whoami, com);
			// sprintf(log_buffer,"[Thread %d] client %d quitted, com closed", whoami, com);
			// SAFELOCK(log_access_mtx);
			// write_to_log(log_buffer);
			// SAFEUNLOCK(log_access_mtx);
			continue;
		}
		request_status = handle_request(com, &request, &client_waiting_lock); // Response is set and log is updated

		if(request_status < 0){
			sprintf(log_buffer,"[Thread %d] Error handling client %d request", whoami, request.client_id);
			SAFELOCK(log_access_mtx);
			write_to_log(log_buffer);
			SAFEUNLOCK(log_access_mtx);
			
			continue;
		}
		
		// printf(ANSI_COLOR_MAGENTA"[Thread %d - client %d]:"ANSI_COLOR_CYAN" %s\n"ANSI_COLOR_RESET, whoami, com, buffer);
		// sprintf(log_buffer,"[Thread %d - client %d]: ", whoami, com);
		// strncat(log_buffer, buffer, 150); // buffer has a size of PIPE_BUF (4096 bytes) 
		// SAFELOCK(log_access_mtx);
		// write_to_log(log_buffer);
		// SAFEUNLOCK(log_access_mtx);
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



ssize_t readn(int fd, void *ptr, size_t n){
	size_t nleft;
	ssize_t nread;

	nleft = n;
	while (nleft > 0)
	{
		if ((nread = read(fd, ptr, nleft)) < 0)
		{
			if (nleft == n)
				return -1; /* error, return -1 */
			else
				break; /* error, return amount read so far */
		}
		else if (nread == 0)
			break; /* EOF */
		nleft -= nread;
		ptr += nread;
	}
	return (n - nleft); /* return >= 0 */
}

ssize_t writen(int fd, void *ptr, size_t n){
	size_t nleft;
	ssize_t nwritten;

	nleft = n;
	while (nleft > 0)
	{
		if ((nwritten = write(fd, ptr, nleft)) < 0)
		{
			if (nleft == n)
				return -1; /* error, return -1 */
			else
				break; /* error, return amount written so far */
		}
		else if (nwritten == 0)
			break;
		nleft -= nwritten;
		ptr += nwritten;
	}
	return (n - nleft); /* return >= 0 */
}

