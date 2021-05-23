#include "server.h"
#include "parser.h"
#include "file.h"
#include "client_queue.h"
#include "log.h"


 

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
extern pthread_cond_t client_is_ready;


extern pthread_mutex_t log_access_mtx;
extern int m_w_pipe[2];

extern void func(clients_list *head);



void clean_request(client_request *request){
	if(request->data != NULL)
		free(request->data);
	if(request->pathname != NULL)
		free(request->pathname);
}

void clean_response(server_response *response){
	if(response->data != NULL)
		free(response->data);
	if(response->filename != NULL)
		free(response->filename);
}


static int handle_request(client_request *request, server_response *response){
	int exit_status = -1;
	char *log_buffer = NULL;
	log_buffer = (char *) calloc(200, sizeof(char));
	if(request->command & OPEN){
		puts(request->pathname);
		puts("halo");
		exit_status = open_file(request->pathname, request->command, request->client_id, response);
		puts("opened");
		
	}
	else if(request->command & READ){
		exit_status = read_file(request->pathname, response);
		if(exit_status == 0){
			sprintf(log_buffer,"Client %d read %d bytes", request->client_id, response->size);
			SAFELOCK(log_access_mtx);
			write_to_log(log_buffer);
			SAFEUNLOCK(log_access_mtx);
		}
	}
	else if(request->command & WRITE){
		exit_status = write_to_file(request->data, request->size, request->pathname, request->client_id, response);
		if(exit_status == 0){
			sprintf(log_buffer,"Client %d wrote %d bytes", request->client_id, request->size);
			SAFELOCK(log_access_mtx);
			write_to_log(log_buffer);
			SAFEUNLOCK(log_access_mtx);
		}
	}
	else if(request->command & APPEND){
		exit_status = append_to_file(request->data, request->size, request->pathname, request->client_id, response);
		if(exit_status == 0){
			sprintf(log_buffer,"Client %d wrote %d bytes", request->client_id, request->size);
			SAFELOCK(log_access_mtx);
			write_to_log(log_buffer);
			SAFEUNLOCK(log_access_mtx);
		}
	}
	free(log_buffer);
	return exit_status;
}




void* worker(void* args){
	int com = 0;
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
	

	fflush(stdout);
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
		printf(ANSI_COLOR_MAGENTA"[Thread %d] received request from client %d\n"ANSI_COLOR_RESET, whoami, com);
		// sprintf(log_buffer,"[Thread %d] received request from client %d", whoami, com);
		// SAFELOCK(log_access_mtx);
		// write_to_log(log_buffer);
		// SAFEUNLOCK(log_access_mtx);
		
	
		CHECKERRNO((read(com, &request, sizeof(request)) < 0), "Reading from client");
		printf("%s\n", request.pathname);
		// if(request.command & QUIT) {
		// 	// close(com); Now I try to send back the com that the client will close to keep track of connecitons/disconnections
		// 	memset(buffer, 0, sizeof(buffer));
		// 	SAFELOCK(free_threads_mtx);
		// 	free_threads[whoami] = true;
		// 	SAFEUNLOCK(free_threads_mtx);
		// 	SAFELOCK(done_queue_mtx);
		// 	insert_client_list(com, &done_queue[0], &done_queue[1]);
		// 	SAFEUNLOCK(done_queue_mtx);
		// 	clean_request(&request);
		// 	clean_response(&respond_to_client);
		// 	// memset(buffer, 0, sizeof(buffer));	
		// 	// printf(ANSI_COLOR_MAGENTA"[Thread %d] client %d quitted, com closed\n"ANSI_COLOR_RESET, whoami, com);
		// 	// sprintf(log_buffer,"[Thread %d] client %d quitted, com closed", whoami, com);
		// 	// SAFELOCK(log_access_mtx);
		// 	// write_to_log(log_buffer);
		// 	// SAFEUNLOCK(log_access_mtx);
		// 	continue;
		// }
		// request_status = handle_request(&request, &respond_to_client); // Response is set and log is updated

		// if(request_status < 0){
		// 	sprintf(log_buffer,"[Thread %d] Error handling client %d request", whoami, request.client_id);
		// 	SAFELOCK(log_access_mtx);
		// 	write_to_log(log_buffer);
		// 	SAFEUNLOCK(log_access_mtx);
		// 	clean_request(&request);
		// 	clean_response(&respond_to_client);
		// 	continue;
		// }
		CHECKERRNO((write(com, &respond_to_client, sizeof(respond_to_client)) < 0), "Writing to client");
		clean_request(&request);
		clean_response(&respond_to_client);
		// printf(ANSI_COLOR_MAGENTA"[Thread %d - client %d]:"ANSI_COLOR_CYAN" %s\n"ANSI_COLOR_RESET, whoami, com, buffer);
		// sprintf(log_buffer,"[Thread %d - client %d]: ", whoami, com);
		// strncat(log_buffer, buffer, 150); // buffer has a size of PIPE_BUF (4096 bytes) 
		// SAFELOCK(log_access_mtx);
		// write_to_log(log_buffer);
		// SAFEUNLOCK(log_access_mtx);
		memset(buffer, 0, sizeof(buffer));
		sprintf(buffer, "%d", com);
		CHECKERRNO((write(m_w_pipe[1], buffer, sizeof(buffer)) < 0), "Return com");
		memset(buffer, 0, sizeof(buffer));	
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





