#include "fssApi.h"
#include "connections.h"

extern int socket_fd;
char open_connection_name[UNIX_MAX_PATH] = "None";

/** TODO:
 * - Make requests to the server -> api don't access directly to the storage
 * - Abstime is the time in 24h format
 * - Check #malloc = #frees file_deleted_request 
*/

// Return value <= 0 to check result with CHECKSCEXITS
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
	return -(n - nleft); /* return <= 0 */
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
	return -(n - nleft); /* return >= 0 */
}

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
	if (strncmp(open_connection_name, "None", UNIX_MAX_PATH) != 0){
		errno = ENFILE;
		return -1;
	}
	memset(open_connection_name, 0, UNIX_MAX_PATH);
	strncpy(sockaddress.sun_path, sockname, UNIX_MAX_PATH);
	strncpy(open_connection_name, sockname, UNIX_MAX_PATH);
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
	if (strncmp(sockname, open_connection_name, UNIX_MAX_PATH) != 0){
		errno = EINVAL;
		return -1;
	}
	memset(open_connection_name, 0, UNIX_MAX_PATH);
	strncpy(open_connection_name, "None", UNIX_MAX_PATH);
	return close(socket_fd);
}

int openFile(const char *pathname, int flags){
	client_request open_request;
	server_response open_response;
	memset(&open_request, 0, sizeof(client_request));
	memset(&open_response, 0, sizeof(server_response));
	open_request.command = flags | OPEN;
	open_request.pathname = (char *) calloc(strlen(pathname)+1, sizeof(char));
	CHECKALLOC(open_request.pathname, "Errore di allocazione openFile");
	strncpy(open_request.pathname, pathname, strlen(pathname));
	open_request.client_id = getpid();
	CHECKSCEXIT(writen(socket_fd, &open_request, sizeof(open_request)), true, "Errore invio richiesta openFile");
	CHECKSCEXIT(readn(socket_fd, &open_response, sizeof(open_response)),true, "Errore lettura risposta server");
	if(open_response.code[0] & FILE_EXISTS){
		errno = FILE_EXISTS;
		clean_request(&open_request);
		clean_response(&open_response);
		return -1; 
	}
	if(open_response.code[0] & FILE_OPERATION_SUCCESS){
		if(flags & O_CREATE){
			puts(ANSI_COLOR_GREEN"File creato e aperto con successo!"ANSI_COLOR_RESET);
			clean_request(&open_request);
			clean_response(&open_response);
			return 0;
		}
		if(flags & O_CREATE & O_LOCK){
			puts(ANSI_COLOR_GREEN"File creato e bloccato con successo!"ANSI_COLOR_RESET);
			clean_request(&open_request);
			clean_response(&open_response);
			return 0;
		}
		if(flags & O_LOCK){
			puts(ANSI_COLOR_GREEN"File aperto e bloccato con successo!"ANSI_COLOR_RESET);
			clean_request(&open_request);
			clean_response(&open_response);
			return 0;
			
		}
		if(flags == 0){
				puts(ANSI_COLOR_GREEN"File aperto con successo!"ANSI_COLOR_RESET);
				clean_request(&open_request);
				clean_response(&open_response);
				return 0;
		}
	}
	else{
		errno = check_error(open_response.code); // Check errno with FILE_*_FAILED
		clean_request(&open_request);
		clean_response(&open_response);
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
	close_request.pathname = (char *) calloc(strlen(pathname)+1, sizeof(char));
	CHECKALLOC(close_request.pathname, "Errore di allocazione openFile");
	close_request.client_id = getpid();
	CHECKSCEXIT(writen(socket_fd, &close_request, sizeof(close_request)), true, "Errore invio richiesta readFile");
	CHECKSCEXIT(readn(socket_fd, &close_response, sizeof(close_response)),true, "Errore lettura risposta server");
	if(close_response.code[0] &FILE_OPERATION_SUCCESS){
		puts("File chiuso con successo!");
		return 0;
	}
	else{
		errno = check_error(close_response.code); // Check errno with FILE_*_FAILED
		clean_request(&close_request);
		clean_response(&close_response);
		return -1; 
	}
}

int readFile(const char *pathname, void **buf, size_t *size){
	client_request read_request;
	server_response read_response;
	unsigned char *data = NULL;
	memset(&read_request, 0, sizeof(client_request));
	memset(&read_response, 0, sizeof(server_response));
	read_request.command = READ;
	read_request.pathname = (char *) calloc(strlen(pathname)+1, sizeof(char));
	CHECKALLOC(read_request.pathname, "Errore di allocazione readFile");
	strncpy(read_request.pathname, pathname, strlen(pathname));
	read_request.client_id = getpid();
	CHECKSCEXIT(writen(socket_fd, &read_request, sizeof(read_request)), true, "Errore invio richiesta readFile");
	CHECKSCEXIT(readn(socket_fd, &read_response, sizeof(read_response)),true, "Errore lettura risposta server");
	if(read_response.code[0] & FILE_OPERATION_SUCCESS){
		*size = read_response.size;
 		data = (unsigned char *) malloc(*size * sizeof(unsigned char));
		memcpy(data, read_response.data, *size);
		* buf = data;
		clean_request(&read_request);
		clean_response(&read_response);
		return 0;
	}
	else{
		errno = check_error(read_response.code);
		clean_request(&read_request);
		clean_response(&read_response);
		return -1;
	}
	return 0;

}


int appendToFile(const char *pathname, void *buf, size_t size, const char *dirname){
	client_request append_request;
	client_request file_deleted_request;
	server_response append_response;
	server_response file_deleted_response;
	memset(&append_request, 0, sizeof(client_request));
	memset(&file_deleted_request, 0, sizeof(client_request));
	memset(&append_response, 0, sizeof(server_response));
	memset(&file_deleted_response, 0, sizeof(server_response));
	append_request.command = APPEND;
	append_request.pathname = (char *) calloc(strlen(pathname)+1, sizeof(char));
	append_request.data = (unsigned char *) calloc(size, sizeof(unsigned char));
	memcpy(append_request.data, buf, size);
	file_deleted_request.command = READ;
	CHECKALLOC(append_request.pathname, "Errore di allocazione appendFile");
	strncpy(append_request.pathname, pathname, strlen(pathname));
	append_request.client_id = getpid();
	file_deleted_request.client_id = getpid();
	CHECKSCEXIT(writen(socket_fd, &append_request, sizeof(append_request)), true, "Errore invio richiesta appendFile");
	CHECKSCEXIT(readn(socket_fd, &append_response, sizeof(append_response)), true, "Errore lettura risposta server");
	if(append_response.code[0] & FILE_OPERATION_SUCCESS){
		clean_request(&append_request);
		clean_response(&append_response);
		clean_request(&file_deleted_request);
		clean_response(&file_deleted_response);
		return 0;
	}
	else{
		errno = check_error(append_response.code);
		clean_request(&append_request);
		clean_response(&append_response);
		clean_request(&file_deleted_request);
		clean_response(&file_deleted_response);
		return -1;
	}
	return 0;

}
// int appendToFile(const char *pathname, void *buf, size_t size, const char *dirname){
// 	int file_index = 0;
// 	unsigned char *buffer = NULL;
// 	file_index = search_file(pathname);
	
// 	if (file_index != -1 && file_index != -EBUSY){
// 		SAFELOCK(server_storage.storage_table[file_index]->file_mutex);
// 		server_storage.storage_table[file_index]->data = (unsigned char *)realloc(server_storage.storage_table[file_index]->data, server_storage.storage_table[file_index]->size + size);
// 		CHECKALLOC(server_storage.storage_table[file_index]->data, "Errore di riallocazione appendToFIle");
// 		buffer = (unsigned char *)buf;
// 		for (int i = server_storage.storage_table[file_index]->size, j = 0; j < size; i++, j++){
// 			server_storage.storage_table[file_index]->data[i] = buffer[j];
// 		}
// 		server_storage.storage_table[file_index]->last_modified = time(NULL);
// 		server_storage.storage_table[file_index]->size += size;
// 		SAFEUNLOCK(server_storage.storage_table[file_index]->file_mutex);
// 		return 0;
// 	}
// 	errno = ENOENT;
// 	return -1;
// }

int writeFile(const char* pathname, const char* dirname){
	struct stat file_info;
	int file, size = 0;
	client_request write_request;
	client_request file_deleted_request;
	server_response write_response;
	server_response file_deleted_response;
	memset(&write_request, 0, sizeof(client_request));
	memset(&file_deleted_request, 0, sizeof(client_request));
	memset(&write_response, 0, sizeof(server_response));
	memset(&file_deleted_response, 0, sizeof(server_response));
	if((file = open("./a.out", O_RDONLY)) == -1){
		perror("Errore durante l'apertura del file");
		return -1;
	}
	if(fstat(file, &file_info) < 0){
		perror("Errore fstat");
		return -1;
	}
	size = file_info.st_size;
	write_request.data = (unsigned char *) calloc(size, sizeof(unsigned char));
	CHECKSCEXIT(readn(file, write_request.data, size), true, "Errore copia del file nella richiesta writeFile");
	
	write_request.command = WRITE;
	write_request.pathname = (char *) calloc(strlen(pathname)+1, sizeof(char));
	file_deleted_request.command = READ;
	CHECKALLOC(write_request.pathname, "Errore di allocazione writeFile");
	strncpy(write_request.pathname, pathname, strlen(pathname));
	write_request.client_id = getpid();
	file_deleted_request.client_id = getpid();
	CHECKSCEXIT(writen(socket_fd, &write_request, sizeof(write_request)), true, "Errore invio richiesta writeFile");
	CHECKSCEXIT(readn(socket_fd, &write_response, sizeof(write_response)), true, "Errore lettura risposta server");
	if(write_response.code[0] & FILE_OPERATION_SUCCESS){
		clean_request(&write_request);
		clean_response(&write_response);
		clean_request(&file_deleted_request);
		clean_response(&file_deleted_response);
		return 0;
	}
	else{
		errno = check_error(write_response.code);
		clean_request(&write_request);
		clean_response(&write_response);
		clean_request(&file_deleted_request);
		clean_response(&file_deleted_response);
		return -1;
	}

	return 0;

}

int removeFile(const char* pathname){
	client_request remove_request;
	server_response remove_response;
	memset(&remove_request, 0, sizeof(client_request));
	memset(&remove_response, 0, sizeof(server_response));
	remove_request.command = REMOVE;
	remove_request.pathname = (char *) calloc(strlen(pathname)+1, sizeof(char));
	CHECKALLOC(remove_request.pathname, "Errore di allocazione readFile");
	strncpy(remove_request.pathname, pathname, strlen(pathname));
	remove_request.client_id = getpid();
	CHECKSCEXIT(writen(socket_fd, &remove_request, sizeof(remove_request)), true, "Errore invio richiesta readFile");
	CHECKSCEXIT(readn(socket_fd, &remove_response, sizeof(remove_response)),true, "Errore lettura risposta server");
	if(remove_response.code[0] & FILE_OPERATION_SUCCESS){
		puts("File rimosso con successo!");
		return 0;
	}
	else{
		errno = check_error(remove_response.code);
		clean_request(&remove_request);
		clean_response(&remove_response);
		return -1;
	}
	return 0;

}

