#include "fssApi.h"
#ifndef CONNECTIONS_H_
#define CONNECTIONS_H_
#include "connections.h"
#endif
#include "serialization.h"

extern int socket_fd;
char open_connection_name[AF_UNIX_MAX_PATH] = "None";
ssize_t read_all_buffer(int com, unsigned char **buffer, size_t* buff_size);
void clean_request(client_request* request);
void clean_response(server_response* response);
int handle_connection(client_request request, server_response *response);
/** TODO:
 * - Make requests to the server -> api don't access directly to the storage
 * - Abstime is the time in 24h format
 * - Check #malloc = #frees file_deleted_request 
*/

static int check_error(unsigned char *code){
	if(code[1] != 0)
		return code[1];
	if(code[0] & FILE_ALREADY_LOCKED)
		return FILE_ALREADY_LOCKED;
	if(code[0] & FILE_EXISTS)
		return FILE_EXISTS;
	if(code[0] & FILE_NOT_EXISTS)
		return ENOENT;
	return 0;
}




int openConnection(const char *sockname, int msec, const struct timespec abstime){
	int i = 1, connection_status = -1;
	struct sockaddr_un sockaddress;
	struct timespec wait_reconnect = {
		.tv_nsec = msec * 1e+6,
		.tv_sec = 0
	};
	struct timespec remaining_until_failure = {
		.tv_nsec = 0,
		.tv_sec = 0
	};
	if (strncmp(open_connection_name, "None", AF_UNIX_MAX_PATH) != 0){
		errno = ENFILE;
		return -1;
	}
	memset(open_connection_name, 0, AF_UNIX_MAX_PATH);
	strncpy(sockaddress.sun_path, sockname, AF_UNIX_MAX_PATH);
	strncpy(open_connection_name, sockname, AF_UNIX_MAX_PATH);
	sockaddress.sun_family = AF_UNIX;
	socket_fd = socket(AF_UNIX, SOCK_STREAM, 0);
	puts(ANSI_COLOR_CYAN "Provo a connettermi...\n" ANSI_COLOR_RESET);
	while ((connection_status = connect(socket_fd, (struct sockaddr *)&sockaddress, sizeof(sockaddress))) == -1){
		nanosleep(&wait_reconnect, NULL);
		if (remaining_until_failure.tv_nsec + wait_reconnect.tv_nsec == 1e+9){
			remaining_until_failure.tv_nsec = 0;
			remaining_until_failure.tv_sec++;
		}
		else
			remaining_until_failure.tv_nsec += wait_reconnect.tv_nsec;
		printf(ANSI_COLOR_RED "\033[ATentativo %d fallito...\n" ANSI_COLOR_RESET, i++);
		if (remaining_until_failure.tv_nsec == abstime.tv_nsec && remaining_until_failure.tv_sec == abstime.tv_sec){
			printf(ANSI_COLOR_RED "Connection timeout\n" ANSI_COLOR_RESET);
			errno = ETIMEDOUT;
			return connection_status;
		}
	}
	return connection_status;
}

int closeConnection(const char *sockname){
	client_request close_request;
	server_response close_response;
	memset(&close_request, 0, sizeof close_request);
	if (strncmp(sockname, open_connection_name, AF_UNIX_MAX_PATH) != 0){
		errno = EINVAL;
		return -1;
	}
	close_request.command = QUIT;
	close_request.client_id = getpid();
	handle_connection(close_request, &close_response);
	return close(socket_fd);
	
}

int openFile(const char *pathname, int flags){
	client_request open_request;
	server_response open_response;
	memset(&open_request, 0, sizeof(client_request));
	memset(&open_response, 0, sizeof(server_response));
	open_request.command = OPEN;
	open_request.flags = flags;
	memset(open_request.pathname, 0, UNIX_MAX_PATH);
	strncpy(open_request.pathname, pathname, UNIX_MAX_PATH);
	open_request.client_id = getpid();
	handle_connection(open_request, &open_response);
	if(open_response.code[0] & FILE_OPERATION_FAILED){
		errno = check_error(open_response.code);
		return -1;
	}
	return 0;
}

int closeFile(const char *pathname){
	client_request close_request;
	server_response close_response;
	memset(&close_request, 0, sizeof(client_request));
	memset(&close_response, 0, sizeof(server_response));
	close_request.command = CLOSE;
	memset(close_request.pathname, 0, UNIX_MAX_PATH);
	strncpy(close_request.pathname, pathname, UNIX_MAX_PATH);
	close_request.client_id = getpid();
	handle_connection(close_request, &close_response);
	if(close_response.code[0] & FILE_OPERATION_FAILED){
		errno = check_error(close_response.code);
		return -1;
	}
	return 0;
}

int readFile(const char *pathname, void **buf, size_t *size){
	client_request read_request;
	server_response read_response;
	memset(&read_request, 0, sizeof(client_request));
	memset(&read_response, 0, sizeof(server_response));
	read_request.command = READ;
	memset(read_request.pathname, 0, UNIX_MAX_PATH);
	strncpy(read_request.pathname, pathname, UNIX_MAX_PATH);
	strncpy(read_request.pathname, pathname, strlen(pathname));
	read_request.client_id = getpid();
	if(handle_connection(read_request, &read_response) < 0){
		errno = ECONNABORTED;
		return -1;
	} 
	if(read_response.code[0] & FILE_OPERATION_SUCCESS){
		if(read_response.size){
			*size = read_response.size;
			*buf = realloc(*buf, *size);
			CHECKALLOC(*buf, "Errore di allocazione buffer lettura");
			memcpy(*buf, read_response.data, *size);
		}
	}
	else{
		errno = check_error(read_response.code);
		clean_request(&read_request);
		return -1;
	}
	clean_request(&read_request);
	clean_response(&read_response);
	return 0;

}


int appendToFile(const char *pathname, void *buf, size_t size, const char *dirname){
	client_request append_request;
	server_response append_response;
	memset(&append_request, 0, sizeof(client_request));
	memset(&append_response, 0, sizeof(server_response));
	append_request.command = APPEND;
	memset(append_request.pathname, 0, UNIX_MAX_PATH);
	strncpy(append_request.pathname, pathname, UNIX_MAX_PATH-1);
	CHECKALLOC(append_request.pathname, "Errore di allocazione appendFile");
	strncpy(append_request.pathname, pathname, strlen(pathname));
	append_request.client_id = getpid();
	append_request.data = (unsigned char *)calloc(size, sizeof(unsigned char));
	memcpy(append_request.data, buf, size);
	handle_connection(append_request, &append_response);
	if(append_response.code[0] & FILE_OPERATION_FAILED){
		errno = check_error(append_response.code);
		clean_request(&append_request);
		return -1;
	}
	clean_request(&append_request);
	return 0;

}

int writeFile(const char* pathname, const char* dirname){
	struct stat file_info;
	int file;
	size_t size = 0, read_bytes = 0;
	client_request write_request;
	server_response write_response;
	memset(&write_request, 0, sizeof(client_request));
	memset(&write_response, 0, sizeof(server_response));
	if((file = open(pathname, O_RDONLY)) == -1){
		perror("Errore durante l'apertura del file");
		return -1;
	}
	if(fstat(file, &file_info) < 0){
		perror("Errore fstat");
		return -1;
	}
	size = file_info.st_size;
	write_request.data = (unsigned char *) calloc(size, sizeof(unsigned char));
	CHECKALLOC(write_request.data, "Errore di allocazione writeFile");
	if((read_bytes = read(file, write_request.data, size)) != size)
		printf("read %lu, size %lu\n", read_bytes, size);

	write_request.command = WRITE;
	strncpy(write_request.pathname, pathname, UNIX_MAX_PATH-1);
	write_request.client_id = getpid();
	write_request.size = size;
	printf("size write_request_file %lu\n", write_request.size);
	puts("writeFile -> handle");
	handle_connection(write_request, &write_response);
	puts("writeFile <- handle");
	if(write_response.code[0] & FILE_OPERATION_FAILED){
		errno = check_error(write_response.code);
		clean_request(&write_request);
		return -1;
	}
	clean_request(&write_request);
	return 0;

}

int removeFile(const char* pathname){
	client_request remove_request;
	server_response remove_response;
	memset(&remove_request, 0, sizeof(client_request));
	memset(&remove_response, 0, sizeof(server_response));
	remove_request.command = REMOVE;
	memset(remove_request.pathname, 0, UNIX_MAX_PATH);
	strncpy(remove_request.pathname, pathname, UNIX_MAX_PATH);
	remove_request.client_id = getpid();
	handle_connection(remove_request, &remove_response);
	if(remove_response.code[0] & FILE_OPERATION_FAILED){
		errno = check_error(remove_response.code);
		return -1;
	}
	return 0;
}


int lockFile(const char* pathname){
	client_request lock_request;
	server_response lock_response;
	memset(&lock_request, 0, sizeof(client_request));
	memset(&lock_response, 0, sizeof(server_response));
	lock_request.command = SET_LOCK;
	lock_request.flags = O_LOCK; // Se il flag e' O_LOCK fa il lock, altrimenti unlock
	memset(lock_request.pathname, 0, UNIX_MAX_PATH);
	strncpy(lock_request.pathname, pathname, UNIX_MAX_PATH);
	lock_request.client_id = getpid();
	handle_connection(lock_request, &lock_response);
	if(lock_response.code[0] & FILE_OPERATION_FAILED){
		errno = check_error(lock_response.code);
		return -1;
	}
	return 0;
}


int unlockFile(const char* pathname){
	client_request unlock_request;
	server_response unlock_response;
	memset(&unlock_request, 0, sizeof(client_request));
	memset(&unlock_response, 0, sizeof(server_response));
	unlock_request.command = SET_LOCK;
	unlock_request.flags = 0; // Se il flag e' O_LOCK fa il lock, altrimenti unlock
	memset(unlock_request.pathname, 0, UNIX_MAX_PATH);
	strncpy(unlock_request.pathname, pathname, UNIX_MAX_PATH);
	unlock_request.client_id = getpid();
	handle_connection(unlock_request, &unlock_response);
	if(unlock_response.code[0] & FILE_OPERATION_FAILED){
		errno = check_error(unlock_response.code);
		return -1;
	}
	return 0;
}

bool send_ack(int com){
	unsigned char acknowledge = 0x01;
	if(write(com, &acknowledge, 1) < 0) return false;
	return true;
}

ssize_t read_all_buffer(int com, unsigned char **buffer, size_t *buff_size){
	ssize_t read_bytes = 0;

	// *buff_size = 1024;
	unsigned char packet_size_buff[sizeof(unsigned long)];
	memset(packet_size_buff, 0, sizeof(unsigned long));
	if (read(com, packet_size_buff, sizeof packet_size_buff) < 0)
		return -1;
	
	*buff_size = char_to_ulong(packet_size_buff);
	printf("\n\nPACKETSIZE = %lu\n\n", *buff_size);
	if(!send_ack(com)) return -1;
	*buffer = realloc(*buffer, *buff_size);
	read_bytes = read(com, *buffer, *buff_size);
	return read_bytes;
}

bool get_ack(int com){
	unsigned char acknowledge = 0;
	if(read(com, &acknowledge, 1) < 0) return false;
	return true;
}

int handle_connection(client_request request, server_response *response){
	unsigned char *buffer = NULL;
	size_t buff_size = 0;
	unsigned char* packet_size_buff = NULL;

	serialize_request(request, &buffer, &buff_size);
	printf("client: %u\ncommand: 0x%.2x\nflags: 0x%.2x\npath: %s\nsize: %lu\n", request.client_id,request.command,request.flags,request.pathname,request.size);

	ulong_to_char(buff_size, &packet_size_buff);

	CHECKRW(writen(socket_fd, packet_size_buff, sizeof(unsigned long)), sizeof(unsigned long), "Errore invio richiesta readFile");
	if(get_ack(socket_fd)){
		CHECKRW(writen(socket_fd, buffer, buff_size), buff_size, "Errore invio richiesta readFile");
		reset_buffer(&buffer, &buff_size);
		puts("Request sent, waiting response...");
		if(read_all_buffer(socket_fd, &buffer, &buff_size) < 0) return -1;
		deserialize_response(response, &buffer, buff_size);
		free(buffer);
		free(packet_size_buff);
		return 0;
	}
	// puts("Deserialized response");
	// printf("code 0: 0x%.2x\ncode 1: 0x%.2x\npath: %s\nsize: %lu\n", response->code[0], response->code[1], response->filename, response->size);
	free(buffer);
	free(packet_size_buff);
	return -1;
}

void clean_request(client_request* request){
	if(request->data == NULL) free(request->data);
}
void clean_response(server_response* response){
	if(response->data == NULL) free(response->data);
}