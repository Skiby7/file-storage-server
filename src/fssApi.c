#include "file.h"
#include "fssApi.h"
#include "connections.h"

extern int socket_fd;
char open_connection_name[UNIX_MAX_PATH] = "None";
extern pthread_mutex_t storage_access_mtx;
extern storage server_storage;

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
	return -(n - nleft); /* return >= 0 */
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

int save_to_file(const char* name, const char* dirname, unsigned char* buffer, int size){
	return 0;
}

void clean_request(client_request *request){
	if(request->data != NULL)
		free(request->data);
	if(request->dirname != NULL)
		free(request->dirname);
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
	open_request.client_id = getpid();
	CHECKSCEXIT(writen(socket_fd, &open_request, sizeof(open_request)), true, "Errore invio richiesta openFile");
	CHECKSCEXIT(readn(socket_fd, &open_response, sizeof(open_response)),true, "Errore lettura risposta server");
	if(open_response.code[0] & FILE_EXISTS){
		errno = FILE_EXISTS;
		clean_request(&open_request);
		clean_response(&open_response);
		return -1; 
	}
	if(open_response.code[1] == 0){
		if(flags & O_CREATE){
			if(open_response.code[0] & FILE_CREATE_SUCCESS & FILE_OPEN_SUCCESS){
				puts(ANSI_COLOR_GREEN"File creato e aperto con successo!"ANSI_COLOR_RESET);
				clean_request(&open_request);
				clean_response(&open_response);
				return 0;
			}

		}
		if(flags & O_CREATE & O_LOCK){
			if(open_response.code[0] & FILE_OPEN_SUCCESS & FILE_LOCK_SUCCESS & FILE_CREATE_SUCCESS){
				puts(ANSI_COLOR_GREEN"File creato e bloccato con successo!"ANSI_COLOR_RESET);
				clean_request(&open_request);
				clean_response(&open_response);
				return 0;
			}
		}
		if(flags & O_LOCK){
			if(open_response.code[0] & FILE_OPEN_SUCCESS & FILE_LOCK_SUCCESS){
				puts(ANSI_COLOR_GREEN"File aperto e bloccato con successo!"ANSI_COLOR_RESET);
				clean_request(&open_request);
				clean_response(&open_response);
				return 0;
			}
		}
		if(flags == 0){
			if(open_response.code[0] & FILE_OPEN_SUCCESS){
				puts(ANSI_COLOR_GREEN"File aperto con successo!"ANSI_COLOR_RESET);
				clean_request(&open_request);
				clean_response(&open_response);
				return 0;
			}
		}

	}
	else{
		errno = open_response.code[1]; // Check errno with FILE_*_FAILED
		clean_request(&open_request);
		clean_response(&open_response);
		return -1; 
	}
}

// int readFile(const char *pathname, void **buf, size_t *size){
// 	int file_index = 0;
// 	unsigned char *data = NULL;

// 	file_index = search_file(pathname);
// 	if (file_index != -1 && file_index != -EBUSY){
// 		SAFELOCK(server_storage.storage_table[file_index]->file_mutex);
// 		*size = server_storage.storage_table[file_index]->size;
// 		data = (unsigned char *)malloc(*size * sizeof(unsigned char));
// 		memcpy(data, server_storage.storage_table[file_index]->data, *size); // check if returns NULL
// 		SAFEUNLOCK(server_storage.storage_table[file_index]->file_mutex);
// 		*buf = data;
// 		return 0;
// 	}
// 	errno = ENOENT;
// 	return -1;
// }

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
	if(read_response.code[0] & FILE_READ_SUCCESS){
		*size = read_response.size;
 		data = (unsigned char *) malloc(*size * sizeof(unsigned char));
		memcpy(data, read_response.data, *size);
		clean_request(&read_request);
		clean_response(&read_response);
		return 0;
	}
	else{
		errno = read_response.code;
		clean_request(&read_request);
		clean_response(&read_response);
		return -1;
	}
}


int appendToFile(const char *pathname, void *buf, size_t size, const char *dirname){
	client_request append_request;
	client_request file_deleted_request;
	server_response append_response;
	server_response file_deleted_response;
	unsigned char *data = NULL;
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
	if(append_response.code[0] & FILE_WRITE_SUCCESS){
		if(append_response.deleted_file && dirname != NULL){
			while(1){
				CHECKSCEXIT(writen(socket_fd, &file_deleted_request, sizeof(file_deleted_request)), true, "Errore invio richiesta file eliminato");
				CHECKSCEXIT(readn(socket_fd, &file_deleted_response, sizeof(file_deleted_response)), true, "Errore invio richiesta file eliminato");
				if(file_deleted_response.code[0] & FILE_READ_SUCCESS){
					save_to_file(file_deleted_response.filename, dirname, file_deleted_response.data, file_deleted_response.size);
					if(file_deleted_response.deleted_file) continue;
					else break;
				}
				else{
					fprintf(stderr, "Errore di ricezione del file eliminato");
					errno = FILE_READ_FAILED;
					clean_request(&append_request);
					clean_response(&append_response);
					clean_request(&file_deleted_request);
					clean_response(&file_deleted_response);
					return -1;
				}
			}
		}
		else{
			clean_request(&append_request);
			clean_response(&append_response);
			clean_request(&file_deleted_request);
			clean_response(&file_deleted_response);
			return 0;
		} 
	}
	else{
		errno = append_response.code;
		clean_request(&append_request);
		clean_response(&append_response);
		clean_request(&file_deleted_request);
		clean_response(&file_deleted_response);
		return -1;
	}

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
	if(write_response.code[0] & FILE_WRITE_SUCCESS){
		if(write_response.deleted_file && dirname != NULL){
			while(1){
				CHECKSCEXIT(writen(socket_fd, &file_deleted_request, sizeof(file_deleted_request)), true, "Errore invio richiesta file eliminato");
				CHECKSCEXIT(readn(socket_fd, &file_deleted_response, sizeof(file_deleted_response)), true, "Errore invio richiesta file eliminato");
				if(file_deleted_response.code[0] & FILE_READ_SUCCESS){
					save_to_file(file_deleted_response.filename, dirname, file_deleted_response.data, file_deleted_response.size);
					if(file_deleted_response.deleted_file) continue;
					else break;
				}
				else{
					fprintf(stderr, "Errore di ricezione del file eliminato");
					errno = FILE_READ_FAILED;
					clean_request(&write_request);
					clean_response(&write_response);
					clean_request(&file_deleted_request);
					clean_response(&file_deleted_response);
					return -1;
				}
			}
		}
		else{
			clean_request(&write_request);
			clean_response(&write_response);
			clean_request(&file_deleted_request);
			clean_response(&file_deleted_response);
			return 0;
		} 
	}
	else{
		errno = write_response.code;
		clean_request(&write_request);
		clean_response(&write_response);
		clean_request(&file_deleted_request);
		clean_response(&file_deleted_response);
		return -1;
	}


}

