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

extern bool *free_threads;
extern pthread_mutex_t free_threads_mtx;

extern pthread_mutex_t ready_queue_mtx;
pthread_mutex_t lock_queue_mtx = PTHREAD_MUTEX_INITIALIZER;
extern pthread_cond_t client_is_ready;


extern pthread_mutex_t log_access_mtx;
extern int good_fd_pipe[2]; // 1 lettura, 0 scrittura
extern int done_fd_pipe[2]; // 1 lettura, 0 scrittura

extern void func(clients_list *head);
ssize_t safe_read(int fd, void *ptr, size_t n);
ssize_t safe_write(int fd, void *ptr, size_t n);
ssize_t read_all_buffer(int com, unsigned char **buffer, size_t* buff_size);
void logger(char *log);


/** TODO:
 * - Port read_all, serialize_response and deserialize_request here to read/write from/to client in one shot
 * - Insert in done_queue broken coms  
*/

int respond_to_client(int com, server_response response){
	int exit_status = -1;
	unsigned char* serialized_response = NULL;
	size_t response_size = 0;
	serialize_response(response, &serialized_response, &response_size);
	exit_status = safe_write(com, serialized_response, response_size);
	reset_buffer(&serialized_response, &response_size);
	return exit_status;
}

int sendback_client(int com, bool done){
	char* buffer = NULL;
	buffer = calloc(PIPE_BUF, sizeof(char));
	sprintf(buffer, "%d", com);
	if(done) write(done_fd_pipe[1], buffer, PIPE_BUF);
	else write(good_fd_pipe[1], buffer, PIPE_BUF);
	free(buffer);
	// printf("SENTBACK %d\n", com);
	return 0;
}

static int handle_request(int com, client_request *request){ // -1 error in file operation -2 error responding to client
	int exit_status = -1, lock_com = 0, lock_id = 0;
	char *log_buffer = NULL;
	server_response response;
	unsigned char* serialized_response = NULL;
	size_t response_size = 0;
	memset(&response, 0, sizeof(response));
	log_buffer = (char *) calloc(LOG_BUFF, sizeof(char));
	printf(ANSI_COLOR_CYAN"##### 0x%.2x #####\n"ANSI_COLOR_RESET, request->command);
	if(request->command & OPEN){
		exit_status = open_file(request->pathname, request->flags, request->client_id, &response);
		if(respond_to_client(com, response) < 0) return -2;
			
	}
	else if(request->command & CLOSE){
		exit_status = close_file(request->pathname, request->client_id, &response); 
		if(respond_to_client(com, response) < 0) return -2;
			
	}
	else if(request->command & READ){
		exit_status = read_file(request->pathname, request->client_id, &response);
		if(respond_to_client(com, response) < 0) return -2;
		if(exit_status == 0){
			snprintf(log_buffer, LOG_BUFF, "Client %d read %lu bytes", request->client_id, response.size);
			logger(log_buffer);
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
	else if(request->command & SET_LOCK){ // TEST THIS
		if(request->flags & O_LOCK){
			exit_status = lock_file(request->pathname, request->client_id, &response);
			if(exit_status == 0){
				if(respond_to_client(com, response) < 0) return -2;
				snprintf(log_buffer, LOG_BUFF, "Client %d locked %s", request->client_id, request->pathname);
				logger(log_buffer);
			}
			else if(response.code[0] | FILE_LOCKED_BY_OTHERS){
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
				
				if(pop_lock_file_list(request->pathname, &lock_id, &lock_com) == 0){
					while (fcntl(lock_com, F_GETFD) != 0 ){
						sendback_client(lock_com, true);
						if(pop_lock_file_list(request->pathname, &lock_id, &lock_com) < 0)
							goto end;
						
					}
					memset(&response, 0, sizeof(response));
					lock_file(request->pathname, lock_id, &response); // Add errorcheck per write su log
					serialize_response(response, &serialized_response, &response_size);
					safe_write(com, serialized_response, response_size);
					reset_buffer(&serialized_response, &response_size);
					
					sendback_client(lock_com, false);
					snprintf(log_buffer, LOG_BUFF, "Client %d locked %s", lock_id, request->pathname);
					logger(log_buffer);
					
				}
			}
			CHECKRW(writen(com, &response, sizeof(response)), sizeof(response), "Writing to client");
			snprintf(log_buffer, LOG_BUFF, "Client %d failed locking %s with error %s", request->client_id, request->pathname, strerror(response.code[1]));
			logger(log_buffer);
		} 
	}
	else if(request->command & QUIT) {
		puts(ANSI_COLOR_BLUE"QUIT REQUEST"ANSI_COLOR_RESET);
		response.code[0] = FILE_OPERATION_SUCCESS;
		if(respond_to_client(com, response) > 0) sendback_client(com, true);
		snprintf(log_buffer, LOG_BUFF, "Client %d quitted", request->client_id);
		free(log_buffer);
		return 0;
	}
end:
	print_storage();
	sendback_client(com, false);
	if(response.data != NULL)
		free(response.data);
	free(log_buffer);
	return exit_status;
}

void* worker(void* args){
	int com = 0;
	size_t request_buffer_size = 0;
	int whoami = *(int*) args;
	char log_buffer[200];
	int request_status = 0;
	unsigned char* request_buffer;
	client_request request;
	memset(log_buffer, 0, 200);
	memset(&request, 0, sizeof(request));
	

	while(true){
		// fflush(stdout);

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
		
		
		puts("Start reading request");
		if(read_all_buffer(com, &request_buffer, &request_buffer_size) < 0){
			sprintf(log_buffer,"[Thread %d] Error handling client %d request", whoami, request.client_id);
			logger(log_buffer);
		}
		puts("Read done");
		deserialize_request(&request, &request_buffer, request_buffer_size);
		reset_buffer(&request_buffer, &request_buffer_size);
		// puts("Deserialized request");
		// printf("client: %u\ncommand: 0x%.2x\nflags: 0x%.2x\npath: %s\nsize: %lu\n", request.client_id,request.command,request.flags,request.pathname,request.size);

		request_status = handle_request(com, &request); // Response is set and log is updated
		printf("REQUEST STATUS: %d\n", request_status);
		if(request.data != NULL){
			free(request.data);
		}
		if(request_status < 0){
			sprintf(log_buffer,"[Thread %d] Error handling client %d request", whoami, request.client_id);
			logger(log_buffer);
			
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
	if((exit_status = write(fd, ptr, n)) < 0){
		sendback_client(fd, true);
		// printf("Sentback %d from safe_write\n", fd);
		return -1;
	}
	return exit_status;
}

ssize_t safe_read(int fd, void *ptr, size_t n){
	int exit_status = 0;
	if((exit_status = read(fd, ptr, n)) < 0){
		sendback_client(fd, true);
		return -1;
	}
	return exit_status;
}


ssize_t read_all_buffer(int com, unsigned char **buffer, size_t *buff_size){
	size_t all_read = 0, packet_size = 0;
	ssize_t read_bytes = 0;
	*buff_size = 1024;
	*buffer = realloc(*buffer, *buff_size);
	unsigned char packet_size_buff[sizeof(unsigned long)];
	memset(packet_size_buff, 0, sizeof(unsigned long));
	while (true){
		if ((read_bytes = safe_read(com, *buffer + all_read, *buff_size - all_read)) < 0)
			return -1;

		all_read += read_bytes;
		if(all_read >= *buff_size){
			*buff_size += 1024;
			*buffer = realloc(*buffer, *buff_size);
			CHECKALLOC(*buffer, "Erorre di riallocazione durante la read dal socket");
		}
		if(packet_size == 0 && read_bytes >= sizeof(unsigned long)){
			memcpy(packet_size_buff, *buffer, sizeof(unsigned long));
			packet_size = char_to_ulong(packet_size_buff);
			
			printf("\n\nPACKETSIZE = %d\n\n", packet_size);
		}
		if(all_read >= packet_size)
			break;
	}
	return all_read; /* return >= 0 */
}

void logger(char *log){
	SAFELOCK(log_access_mtx);
	write_to_log(log);
	SAFEUNLOCK(log_access_mtx);
}
