#include "server.h"
#include "parser.h"
#include "file.h"
#include "client_queue.h"
#include "log.h"
#include "serialization.h"
#define LOG_BUFF 512

 

extern config configuration; // Server config
extern volatile sig_atomic_t can_accept;
extern volatile sig_atomic_t abort_connections;
extern pthread_mutex_t abort_connections_mtx;
extern clients_list *ready_queue[2];

extern bool *free_threads;
extern pthread_mutex_t free_threads_mtx;

extern pthread_mutex_t ready_queue_mtx;
extern pthread_cond_t client_is_ready;


extern pthread_mutex_t log_access_mtx;
extern int good_fd_pipe[2]; // 1 lettura, 0 scrittura
extern int done_fd_pipe[2]; // 1 lettura, 0 scrittura

extern void func(clients_list *head);
ssize_t safe_read(int fd, void *ptr, size_t n);
ssize_t safe_write(int fd, void *ptr, size_t n);
ssize_t read_all_buffer(int com, unsigned char **buffer, size_t* buff_size);
void logger(char *log);
void add_line();

extern pthread_mutex_t lines_mtx;
extern int lines;


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
	if (safe_write(com, packet_size_buff, sizeof(unsigned long)) < 0)
		return -1;
	
	if(get_ack(com)) exit_status = safe_write(com, serialized_response, response_size);
	free(serialized_response);
	return exit_status;
}

int sendback_client(int com, bool done){
	char* buffer = NULL;
	buffer = calloc(PIPE_BUF+1, sizeof(char));
	sprintf(buffer, "%d", com);
	if(done) write(done_fd_pipe[1], buffer, PIPE_BUF);
	else write(good_fd_pipe[1], buffer, PIPE_BUF);
	free(buffer);
	// if(done) printf("SENTBACK BROKEN COM %d\n", com);
	return 0;
}

void lock_next(char* pathname, bool mutex_write){
	int lock_com = 0, lock_id = 0;
	server_response response;
	char *log_buffer = (char *) calloc(LOG_BUFF+1, sizeof(char));
	memset(&response, 0, sizeof response);
	if(pop_lock_file_list(pathname, &lock_id, &lock_com) == 0){
		while (fcntl(lock_com, F_GETFD) != 0 ){
			sendback_client(lock_com, true);
			if(pop_lock_file_list(pathname, &lock_id, &lock_com) < 0){
				free(log_buffer);
				return;
			}
		}
		
		if(lock_file(pathname, lock_id, mutex_write, &response) < 0){
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
	char *log_buffer = (char *) calloc(LOG_BUFF+1, sizeof(char));
	char* last_file = NULL;
	server_response response;
	memset(&response, 0, sizeof(response));
	sprintf(log_buffer, "[ Thread %d ] Serving client %d", thread, request->client_id);
	logger(log_buffer);
	// printf(ANSI_COLOR_CYAN"##### 0x%.2x #####\n"ANSI_COLOR_RESET, request->command);
	if(request->command & OPEN){
		exit_status = open_file(request->pathname, request->flags, request->client_id, &response);
		if(respond_to_client(com, response) < 0) return -2;
		
		if(request->flags & O_LOCK){
			snprintf(log_buffer, LOG_BUFF, "Client %d open-locked %s",request->client_id, request->pathname);
			logger(log_buffer);

		} 
		else{
			snprintf(log_buffer, LOG_BUFF, "Client %d opened %s",request->client_id, request->pathname);
			logger(log_buffer);
		}
			
	}
	else if(request->command & CLOSE){
		exit_status = close_file(request->pathname, request->client_id, &response); 
		if(respond_to_client(com, response) < 0) return -2;
		snprintf(log_buffer, LOG_BUFF, "Client %d closed %s",request->client_id, request->pathname);
		logger(log_buffer);
			
	}
	else if(request->command & READ){
		if(request->files_to_read == 1){
			exit_status = read_file(request->pathname, request->client_id, &response);
			if(respond_to_client(com, response) < 0) return -2;
			if(exit_status == 0){
				snprintf(log_buffer, LOG_BUFF, "Client %d read %lu bytes", request->client_id, response.size);
				logger(log_buffer);
			}
		}
		else{
			while(read_n_file(&last_file, request->client_id, &response) != 1 && (!request->files_to_read || files_read < request->files_to_read)){
					respond_to_client(com, response);
					get_ack(com);
					clean_response(&response);
					memset(&response, 0, sizeof response);
					files_read++;
			}
			respond_to_client(com, response);
			clean_response(&response);
			snprintf(log_buffer, LOG_BUFF, "Client %d read %d files", request->client_id, files_read);
			logger(log_buffer);
			if(last_file){
				free(last_file);
				last_file = NULL;
			} 
			exit_status = 0;
		}
	}
	else if(request->command & WRITE){
		exit_status = write_to_file(request->data, request->size, request->pathname, request->client_id, &response);
		if(respond_to_client(com, response) < 0) return -2;
		if(exit_status == 0){
			snprintf(log_buffer, LOG_BUFF, "Client %d wrote %lu bytes", request->client_id, request->size);
			logger(log_buffer);		
		} 
	}
	else if(request->command & APPEND){
		exit_status = append_to_file(request->data, request->size, request->pathname, request->client_id, &response);
		if(respond_to_client(com, response) < 0) return -2;
		if(exit_status == 0){
			snprintf(log_buffer, LOG_BUFF, "Client %d wrote %lu bytes", request->client_id, request->size);
			logger(log_buffer);
		} 
	}
	else if(request->command & REMOVE){
		exit_status = remove_file(request->pathname, request->client_id, &response);
		if(respond_to_client(com, response) < 0) return -2;
		if(exit_status == 0){
			snprintf(log_buffer, LOG_BUFF, "Client %d deleted %s", request->client_id, request->pathname);
			logger(log_buffer);
		} 
	}
	else if(request->command & SET_LOCK){ // TEST THIS
		// printf("REQUEST COMMAND: 0x%.2x\nREQUEST FLAGS: 0x%.2x\n", request->command, request->flags);
		if(request->flags & O_LOCK){
			exit_status = lock_file(request->pathname, request->client_id, true, &response);
			if(exit_status == 0){
				if(respond_to_client(com, response) < 0) return -2;
				snprintf(log_buffer, LOG_BUFF, "Client %d locked %s", request->client_id, request->pathname);
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
				if(respond_to_client(com, response) < 0) return -2;
				snprintf(log_buffer, LOG_BUFF, "Client %d failed locking %s with error %s", request->client_id, request->pathname, strerror(response.code[1]));
				logger(log_buffer);
			}
		}
		else{
			exit_status = unlock_file(request->pathname, request->client_id, &response);
			if(respond_to_client(com, response) < 0) return -2;
			if(exit_status == 0){
				snprintf(log_buffer, LOG_BUFF, "Client %d unlocked %s", request->client_id, request->pathname);
				logger(log_buffer);
				lock_next(request->pathname, true);
			}
			else{
				respond_to_client(com, response);
				safe_write(com, &response, sizeof response);
				snprintf(log_buffer, LOG_BUFF, "Client %d failed unlocking %s with error %s", request->client_id, request->pathname, strerror(response.code[1]));
				logger(log_buffer);
			}
		} 
	}

	if(configuration.tui){
		print_storage_info();
		add_line();
	} 
	// print_storage();
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
	
	while(true){
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
		// for (size_t i = 0; i < request_buffer_size; i++)
		// 	printf("%.2x ", request_buffer[i]);
		// puts("");
		deserialize_request(&request, &request_buffer, request_buffer_size);

		
		// reset_buffer(&request_buffer, &request_buffer_size);
		// printf(ANSI_COLOR_MAGENTA"[Thread %d] received request from client %d"ANSI_COLOR_RESET_N, whoami, request.client_id);
		// if(configuration.tui) add_line();
		// puts("Deserialized request");
		// printf("client: %u\ncommand: 0x%.2x\nflags: 0x%.2x\nfiles_to_read: %d\npath: %s\nsize: %lu\n", request.client_id,request.command,request.flags, request.files_to_read, request.pathname,request.size);

		request_status = handle_request(com, whoami, &request); // Response is set and log is updated
		// printf("REQUEST COMMAND: 0x%.2x\nREQUEST STATUS: %d\n",request.command, request_status);
		clean_request(&request);
		
		if(request_status < 0){
			sprintf(log_buffer,"[Thread %d] Error handling client %d request", whoami, request.client_id);
			logger(log_buffer);
			SAFELOCK(free_threads_mtx);
			free_threads[whoami] = true;
			SAFEUNLOCK(free_threads_mtx);
			continue;
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
		sendback_client(fd, true);
		// printf("Sentback %d from safe_write\n", fd);
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
		// puts(ANSI_COLOR_BLUE"QUIT REQUEST"ANSI_COLOR_RESET);
		sendback_client(com, true);
		return -2;
	}
	if(!send_ack(com)) return -1;
	*buffer = calloc(*buff_size, sizeof(unsigned char));
	CHECKALLOC(*buffer, "Errore allocazione buffer read");
	
	
	read_bytes = safe_read(com, *buffer, *buff_size);
	// for (size_t i = 0; i < *buff_size; i++)
	// 	printf("%.2x ", (*buffer)[i]);
	
	return read_bytes;
}

void logger(char *log){
	SAFELOCK(log_access_mtx);
	write_to_log(log);
	SAFEUNLOCK(log_access_mtx);
}

void add_line(){
	SAFELOCK(lines_mtx);
	lines++;
	SAFEUNLOCK(lines_mtx);
}
