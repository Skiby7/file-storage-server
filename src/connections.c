#include "server.h"
#include "parser.h"
#include "file.h"
#include "client_queue.h"
#include "log.h"
#include "serialization.h"
#define LOG_BUFF 512
#define TUI_BUFF 20
 
pthread_mutex_t tui_mtx = PTHREAD_MUTEX_INITIALIZER;
extern config configuration; // Server config
extern bool abort_connections;
extern pthread_mutex_t abort_connections_mtx;
extern clients_list *ready_queue[2];

extern bool *free_threads;
extern pthread_mutex_t free_threads_mtx;

extern pthread_mutex_t ready_queue_mtx;
extern pthread_cond_t client_is_ready;


extern pthread_mutex_t log_access_mtx;
extern int good_fd_pipe[2]; // 1 lettura, 0 scrittura
extern int done_fd_pipe[2]; // 1 lettura, 0 scrittura
extern int tui_pipe[2];
extern void func(clients_list *head);
ssize_t safe_read(int fd, void *ptr, size_t n);
ssize_t safe_write(int fd, void *ptr, size_t n);
ssize_t read_all_buffer(int com, unsigned char **buffer, size_t* buff_size);

void logger(char *log);



bool get_ack(int com){
	unsigned char acknowledge = 0;
	if(safe_read(com, &acknowledge, 1) < 0) return false;
	return true;
}

int respond_to_client(int com, server_response response){
	int exit_status = -1;
	unsigned char* serialized_response = NULL;
	unsigned long response_size = 0;
	unsigned char packet_size_buff[sizeof(unsigned long)];
	serialize_response(response, &serialized_response, &response_size);
	ulong_to_char(response_size, packet_size_buff);
	if (safe_write(com, packet_size_buff, sizeof packet_size_buff) < 0)
		return -1;
	
	if(get_ack(com)) exit_status = safe_write(com, serialized_response, response_size);
	free(serialized_response);
	return exit_status;
}

int sendback_client(int com, bool done){
	char* buffer = NULL;
	buffer = calloc(PIPE_BUF+1, sizeof(char));
	sprintf(buffer, "%d", com);
	
	if(done){ CHECKSCEXIT(write(done_fd_pipe[1], buffer, PIPE_BUF), true, "Errore write done_fd_pipe sendback_client"); }
	else{ CHECKSCEXIT(write(good_fd_pipe[1], buffer, PIPE_BUF), true, "Errore write good_fd_pipe sendback_client"); }
	free(buffer);
	return 0;
}

void lock_next(char* pathname, bool server_mutex, bool file_mutex){
	int lock_com = 0, lock_id = 0;
	server_response response;
	char *log_buffer = (char *) calloc(LOG_BUFF+1, sizeof(char));
	memset(&response, 0, sizeof response);
	if(pop_lock_file_list(pathname, &lock_id, &lock_com, server_mutex, file_mutex) == 0){
		while (fcntl(lock_com, F_GETFD) != 0 ){
			sendback_client(lock_com, true);
			if(pop_lock_file_list(pathname, &lock_id, &lock_com, server_mutex, file_mutex) < 0){
				free(log_buffer);
				return;
			}
		}
		
		if(lock_file(pathname, lock_id, server_mutex, file_mutex, &response) < 0){
			snprintf(log_buffer, LOG_BUFF, "Client %d, error locking %s", lock_id, pathname);
			logger(log_buffer);
		}
		else{
			snprintf(log_buffer, LOG_BUFF, "Client %d locked %s", lock_id, pathname);
			logger(log_buffer);
		} 	
		respond_to_client(lock_com, response);
		sendback_client(lock_com, false);
	
	}
	free(log_buffer);
}


static int handle_request(int com, int thread, client_request *request){ // -1 error in file operation -2 error responding to client
	int exit_status = -1, files_read = 0;
	char* log_buffer = (char *) calloc(LOG_BUFF+1, sizeof(char));
	char tui_buffer[TUI_BUFF] = {0};
	char* last_file = NULL;
	server_response response;
	victim_queue *victims = NULL, *befree = NULL;
	memset(&response, 0, sizeof(response));
	sprintf(log_buffer, "[ Thread %d ] Serving client %d", thread, request->client_id);
	logger(log_buffer);
	if(configuration.tui){
		if(request->command & REMOVE || request->command & WRITE || request->command & APPEND){
			strcpy(tui_buffer, "WRITE");
			CHECKERRNO(write(tui_pipe[1], tui_buffer, TUI_BUFF) < 0, "Errore tui pipe");
		}
		else if(request->command & READ || request->command & READ_N){
			strcpy(tui_buffer, "READ");
			CHECKERRNO(write(tui_pipe[1], tui_buffer, TUI_BUFF) < 0, "Errore tui pipe");
		}
	}
	if(request->command & OPEN){
		exit_status = open_file(request->pathname, request->flags, request->client_id, &response);
		if(respond_to_client(com, response) < 0){
			clean_response(&response);
			free(log_buffer);
			return -2;
		}
		if(exit_status == 0){
			if(request->flags & O_LOCK){
				snprintf(log_buffer, LOG_BUFF, "Client %d Open-locked %s",request->client_id, request->pathname);
				logger(log_buffer);

			} 
			else{
				snprintf(log_buffer, LOG_BUFF, "Client %d Opened %s",request->client_id, request->pathname);
				logger(log_buffer);
			}
		}
			
	}
	else if(request->command & CLOSE){
		exit_status = close_file(request->pathname, request->client_id, &response); 
		if(respond_to_client(com, response) < 0){
			clean_response(&response);
			free(log_buffer);
			return -2;
		}
		snprintf(log_buffer, LOG_BUFF, "Client %d Closed %s",request->client_id, request->pathname);
		logger(log_buffer);
			
	}
	else if(request->command & READ){
		exit_status = read_file(request->pathname, request->client_id, &response);
		if(respond_to_client(com, response) < 0){
			clean_response(&response);
			free(log_buffer);
			return -2;
		}
		if(exit_status == 0){
			snprintf(log_buffer, LOG_BUFF, "Client %d Read %lu bytes", request->client_id, response.size);
			logger(log_buffer);
		}
	}
	else if(request->command & READ_N){
		while(!request->files_to_read || files_read != request->files_to_read){
			if(read_n_file(&last_file, request->client_id, &response) == 1) break;
			if(respond_to_client(com, response) < 0){
				clean_response(&response);
				free(log_buffer);
				return -2;
			}
			snprintf(log_buffer, LOG_BUFF, "Client %d Read %lu bytes", request->client_id, response.size);
			logger(log_buffer);
			if(!get_ack(com)){
				clean_response(&response);
				free(log_buffer);
				return -2;
			}
			clean_response(&response);
			memset(&response, 0, sizeof response);
			files_read++;
		}
		if(files_read == request->files_to_read){
			clean_response(&response);
			response.code[0] = FILE_NOT_EXISTS;
		}
		if(last_file){
			free(last_file);
			last_file = NULL;
		} 
		if(respond_to_client(com, response) < 0){
			clean_response(&response);
			free(log_buffer);
			return -2;
		}
		clean_response(&response);
		// snprintf(log_buffer, LOG_BUFF, "Client %d read %d files", request->client_id, files_read);
		// logger(log_buffer);
		
		exit_status = 0;
	
	}
	else if(request->command & WRITE){
		exit_status = write_to_file(request->data, request->size, request->pathname, request->client_id, &response, &victims);
		if(respond_to_client(com, response) < 0){
			clean_response(&response);
			free(log_buffer);
			return -2;
		}
		if(exit_status == 0){
			snprintf(log_buffer, LOG_BUFF, "Client %d Wrote %lu bytes", request->client_id, request->size);
			logger(log_buffer);
			if(victims && get_ack(com)){
				while(victims){
					respond_to_client(com, victims->victim);
					clean_response(&victims->victim);
					befree = victims;
					victims = victims->next;
					free(befree);
					get_ack(com);
				}
				clean_response(&response);
				response.code[0] = FILE_OPERATION_SUCCESS;
				response.data = (unsigned char *) calloc(1, sizeof(unsigned char));
				response.size = 1;
				respond_to_client(com, response);
			}		
		} 
	}
	else if(request->command & APPEND){
		exit_status = append_to_file(request->data, request->size, request->pathname, request->client_id, &response, &victims);
		if(respond_to_client(com, response) < 0){
			clean_response(&response);
			free(log_buffer);
			return -2;
		}
		if(exit_status == 0){
			snprintf(log_buffer, LOG_BUFF, "Client %d Wrote %lu bytes", request->client_id, request->size);
			logger(log_buffer);
			if(victims && get_ack(com)){
				while(victims){
					respond_to_client(com, victims->victim);
					clean_response(&victims->victim);
					befree = victims;
					victims = victims->next;
					free(befree);
					get_ack(com);
				}
				clean_response(&response);
				response.code[0] = FILE_OPERATION_SUCCESS;
				response.data = (unsigned char *) calloc(1, sizeof(unsigned char));
				response.size = 1;
				respond_to_client(com, response);
			}
		} 
	}
	else if(request->command & REMOVE){
		exit_status = remove_file(request->pathname, request->client_id, &response);
		if(respond_to_client(com, response) < 0){
			clean_response(&response);
			free(log_buffer);
			return -2;
		}
		if(exit_status == 0){
			snprintf(log_buffer, LOG_BUFF, "Client %d Deleted %s", request->client_id, request->pathname);
			logger(log_buffer);
		} 
	}
	else if(request->command & SET_LOCK){ // TEST THIS
		if(request->flags & O_LOCK){
			exit_status = lock_file(request->pathname, request->client_id, true, true, &response);
			if(exit_status == 0){
				if(respond_to_client(com, response) < 0){
					clean_response(&response);
					free(log_buffer);
					return -2;
				}
				snprintf(log_buffer, LOG_BUFF, "Client %d Locked %s", request->client_id, request->pathname);
				logger(log_buffer);
			}
			else if(response.code[0] & FILE_LOCKED_BY_OTHERS){
				insert_lock_file_list(request->pathname, request->client_id, com);
				snprintf(log_buffer, LOG_BUFF, "Client %d waiting on %s", request->client_id, request->pathname);
				logger(log_buffer);
				free(log_buffer);
				return 0;
			}
			else{
				if(respond_to_client(com, response) < 0){
					clean_response(&response);
					free(log_buffer);
					return -2;
				}
				snprintf(log_buffer, LOG_BUFF, "Client %d failed locking %s with error %s", request->client_id, request->pathname, strerror(response.code[1]));
				logger(log_buffer);
			}
		}
		else{
			exit_status = unlock_file(request->pathname, request->client_id, &response);
			if(respond_to_client(com, response) < 0){
				clean_response(&response);
				free(log_buffer);
				return -2;
			} 
			if(exit_status == 0){
				snprintf(log_buffer, LOG_BUFF, "Client %d Unlocked %s", request->client_id, request->pathname);
				logger(log_buffer);
				lock_next(request->pathname, true, true);
			}
			else{
				snprintf(log_buffer, LOG_BUFF, "Client %d failed unlocking %s -> %s", request->client_id, request->pathname, strerror(response.code[1]));
				logger(log_buffer);
			}
		} 
	}
	
	if(configuration.tui){
		if(request->command & REMOVE || request->command & WRITE || request->command & APPEND){
			strcpy(tui_buffer, "WRITE END");
			CHECKERRNO(write(tui_pipe[1], tui_buffer, TUI_BUFF) < 0, "Errore tui pipe");
		}
		else if(request->command & READ || request->command & READ_N){
			strcpy(tui_buffer, "READ END");
			CHECKERRNO(write(tui_pipe[1], tui_buffer, TUI_BUFF) < 0, "Errore tui pipe");
		}
	}
	sendback_client(com, false);
	clean_response(&response);
	free(log_buffer);
	return exit_status;
}

void* worker(void* args){
	int com = 0;
	size_t request_buffer_size = 0;
	int whoami = *(int*) args;
	char log_buffer[LOG_BUFF];
	int request_status = 0;
	ssize_t read_status = 0;
	unsigned char* request_buffer = NULL;
	client_request request;
	memset(log_buffer, 0, LOG_BUFF);
	pthread_setcancelstate(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);
	while(true){
		// Thread waits for work to be assigned

		SAFELOCK(ready_queue_mtx);
		while(ready_queue[1] == NULL){
			CHECKEXIT(pthread_cond_wait(&client_is_ready, &ready_queue_mtx) != 0, false, "Errore cond_wait worker");
		}
		SAFELOCK(free_threads_mtx);
		free_threads[whoami] = false;
		SAFEUNLOCK(free_threads_mtx);
		com = pop_client(&ready_queue[0], &ready_queue[1]); // Pop dalla lista dei socket ready che va fatta durante il lock		
		SAFEUNLOCK(ready_queue_mtx);
		if(com == -1){ // Falso allarme
			SAFELOCK(free_threads_mtx);
			free_threads[whoami] = false;
			SAFEUNLOCK(free_threads_mtx);
			continue;
		}
		if(com == -2){
			SAFELOCK(free_threads_mtx);
			free_threads[whoami] = false;
			SAFEUNLOCK(free_threads_mtx);
			return NULL;
		}
		memset(&request, 0, sizeof request);
		read_status = read_all_buffer(com, &request_buffer, &request_buffer_size);
		if(read_status < 0){
			if(read_status == -1){
				sprintf(log_buffer,"[Thread %d] Error handling client with fd %d request", whoami, com);
				logger(log_buffer);
			}
			SAFELOCK(free_threads_mtx);
			free_threads[whoami] = true;
			SAFEUNLOCK(free_threads_mtx);
			continue;
		}
		deserialize_request(&request, &request_buffer, request_buffer_size);

		
		
		request_status = handle_request(com, whoami, &request); // Response is set and log is updated
		clean_request(&request);
		
		if(request_status < 0){
			sprintf(log_buffer,"Error handling client %d request", request.client_id);
			logger(log_buffer);
			SAFELOCK(free_threads_mtx);
			free_threads[whoami] = true;
			SAFEUNLOCK(free_threads_mtx);
			continue;
		}
		SAFELOCK(free_threads_mtx);
		free_threads[whoami] = true;
		SAFEUNLOCK(free_threads_mtx);

	}
	return (void *) 0;
}

ssize_t safe_write(int fd, void *ptr, size_t n){
	int exit_status = 0;
	if((exit_status = writen(fd, ptr, n)) < 0){
		sendback_client(fd, true);
		return -1;
	}
	return exit_status;
}

ssize_t safe_read(int fd, void *ptr, size_t n){
	int exit_status = 0;
	if((exit_status = readn(fd, ptr, n)) < 0){
		sendback_client(fd, true);
		return -1;
	}
	return exit_status;
}

bool send_ack(int com){
	unsigned char acknowledge = 0x01;
	if(safe_write(com, &acknowledge, 1) < 0) return false;
	return true;
}

ssize_t read_all_buffer(int com, unsigned char **buffer, size_t *buff_size){
	ssize_t read_bytes = 0;
	unsigned char packet_size_buff[sizeof(unsigned long)];
	memset(packet_size_buff, 0, sizeof(unsigned long));
	
	if (safe_read(com, packet_size_buff, sizeof packet_size_buff) < 0)
		return -1;

	*buff_size = char_to_ulong(packet_size_buff);
	
	if(*buff_size == 0) {
		sendback_client(com, true);
		return -2;
	}
	if(!send_ack(com)) return -1;
	*buffer = calloc(*buff_size, sizeof(unsigned char));
	CHECKALLOC(*buffer, "Errore allocazione buffer read");
	
	
	read_bytes = safe_read(com, *buffer, *buff_size);
	
	return read_bytes;
}

void logger(char *log){
	SAFELOCK(log_access_mtx);
	write_to_log(log);
	SAFEUNLOCK(log_access_mtx);
}

/**
 * If enabled, this routine will print some informations on the stdout.
 * This is more efficient than making each thread print the info everytime it completes a task, 
 * because this way we access the storage to retreive the informations only when a write is finished.
 * 
*/

void* print_tui(void *args){
	struct pollfd *pipe_poll =  (struct pollfd *) malloc(sizeof(struct pollfd));
	nfds_t count = 1;
	int poll_val = 0, read_bytes = 0;
	pipe_poll[0].fd = tui_pipe[0];
	pipe_poll[0].events = POLLIN;
	char buffer[20] = {0};
	char *stat_buff = NULL;
	bool read_stat = false, write_stat = false;
	printf("\n\n\n");
	stat_buff = print_storage_info();
	while(true){
		poll_val = poll(pipe_poll, count, -1); 
		if(poll_val < 0) continue;
		if(pipe_poll[0].revents & POLLIN){
			read_bytes = read(tui_pipe[0], buffer, sizeof buffer);
			CHECKERRNO((read_bytes < 0), "Errore durante la lettura della pipe");
			if(strcmp(buffer, "QUIT") == 0) break;
			if(strcmp(buffer, "READ") == 0)
				read_stat = true;
			if(strcmp(buffer, "WRITE") == 0)
				write_stat = true;
			if(strcmp(buffer, "READ END") == 0)
				read_stat = false;
			if(strcmp(buffer, "WRITE END") == 0){
				write_stat = false;
				if (stat_buff){
					free(stat_buff);
					stat_buff = NULL;
				}
				stat_buff = print_storage_info();
			}
				
			printf("\033[3A");
			if(read_stat) 
				printf("READ("ANSI_COLOR_CYAN"*"ANSI_COLOR_RESET")");
			else printf("READ( )");
			printf(" - ");
			if(write_stat) 
				printf("WRITE("ANSI_COLOR_CYAN"*"ANSI_COLOR_RESET")\n");
			else printf("WRITE( )\n");
			printf("%s",stat_buff);
		}
	}
	return NULL;
}