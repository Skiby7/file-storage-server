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
int save_to_file(const char* pathname, unsigned char* data, size_t size);
bool send_ack(int com){
	unsigned char acknowledge = 0x01;
	if(write(com, &acknowledge, 1) < 0) return false;
	return true;
}

static int check_error(unsigned char *code){
	if(code[1] != 0)
		return code[1];
	if(code[0] & FILE_ALREADY_LOCKED)
		return EPERM;
	if(code[0] & FILE_LOCKED_BY_OTHERS)
		return EBUSY;
	if(code[0] & FILE_EXISTS)
		return EEXIST;
	if(code[0] & FILE_NOT_EXISTS)
		return ENOENT;
	
	return 0;
}

static int check_path(const char* pathname, char* op){
	if(!pathname){
		// printf(ANSI_COLOR_RED"Errore API: pathname non specificato!\n"ANSI_COLOR_RESET);
		errno = EINVAL;
		return -1;
	}
	if(pathname[0] != '/'){
		// printf(ANSI_COLOR_RED"Errore API: il pathname %s non è assoluto. %s non completata, fornire pathname assoluto!\n"ANSI_COLOR_RESET, pathname, op);
		errno = EINVAL;
		return -1;
	}
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
		errno = ENOENT;
		return -1;
	}
	memset(open_connection_name, 0, AF_UNIX_MAX_PATH);
	strncpy(sockaddress.sun_path, sockname, AF_UNIX_MAX_PATH);
	strncpy(open_connection_name, sockname, AF_UNIX_MAX_PATH);
	sockaddress.sun_family = AF_UNIX;
	socket_fd = socket(AF_UNIX, SOCK_STREAM, 0);
	while ((connection_status = connect(socket_fd, (struct sockaddr *)&sockaddress, sizeof(sockaddress))) == -1){
		nanosleep(&wait_reconnect, NULL);
		if (remaining_until_failure.tv_nsec + wait_reconnect.tv_nsec == 1e+9){
			remaining_until_failure.tv_nsec = 0;
			remaining_until_failure.tv_sec++;
		}
		else
			remaining_until_failure.tv_nsec += wait_reconnect.tv_nsec;
		printf(ANSI_COLOR_RED "\033[ATentativo %d fallito..." ANSI_COLOR_RESET_N, i++);
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
	memset(&close_request, 0, sizeof close_request);
	if (strncmp(sockname, open_connection_name, AF_UNIX_MAX_PATH) != 0){
		errno = EINVAL;
		return -1;
	}
	unsigned char packet_size_buff[sizeof(unsigned long)];
	ulong_to_char(0, packet_size_buff);	
	writen(socket_fd, packet_size_buff, sizeof packet_size_buff);
	return close(socket_fd);
	
}

int openFile(const char *pathname, int flags){
	if(check_path(pathname, "openFile") < 0) return -1;
	client_request open_request;
	server_response open_response;
	memset(&open_response, 0, sizeof(server_response));
	init_request(&open_request, getpid(), OPEN, flags, pathname);
	handle_connection(open_request, &open_response);
	clean_request(&open_request);
	if(open_response.code[0] & FILE_OPERATION_FAILED){
		errno = check_error(open_response.code);
		clean_response(&open_response);
		return -1;
	}
	clean_response(&open_response);
	return 0;
}

int closeFile(const char *pathname){
	if(check_path(pathname, "closeFile") < 0) return -1;
	client_request close_request;
	server_response close_response;
	memset(&close_response, 0, sizeof(server_response));
	init_request(&close_request, getpid(), CLOSE, 0, pathname);
	handle_connection(close_request, &close_response);
	clean_request(&close_request);
	if(close_response.code[0] & FILE_OPERATION_FAILED){
		errno = check_error(close_response.code);
		clean_response(&close_response);
		return -1;
	}
	clean_response(&close_response);
	return 0;
}

int readFile(const char *pathname, void **buf, size_t *size){
	if(check_path(pathname, "readFile") < 0) return -1;
	client_request read_request;
	server_response read_response;
	unsigned char *data = NULL;
	memset(&read_response, 0, sizeof(server_response));
	init_request(&read_request, getpid(), READ, 0, pathname);
	if(handle_connection(read_request, &read_response) < 0){
		errno = ECONNABORTED;
		clean_request(&read_request);
		clean_response(&read_response);
		return -1;
	} 
	if(read_response.code[0] & FILE_OPERATION_SUCCESS){
		if(read_response.size){
			*size = read_response.size;
			data = calloc(*size, sizeof(unsigned char));
			CHECKALLOC(data, "Errore di allocazione buffer lettura");
			memcpy(data, read_response.data, *size);
			*buf = data;
		}
	}
	else{
		errno = check_error(read_response.code);
		clean_request(&read_request);
		clean_response(&read_response);
		return -1;
	}
	clean_request(&read_request);
	clean_response(&read_response);
	return 0;
}

int readNFile(int N, const char* dirname){
	client_request read_n_request;
	server_response read_n_response;
	bool good_path = (check_path(dirname, "readNFile") == 0) ? true : false;
	int i = 0; // First file is read outside the loop
	unsigned char* buffer = NULL;
	size_t buff_size = 0;
	char current_dir[PATH_MAX] = {0};
	if(good_path){ CHECKEXIT(getcwd(current_dir, sizeof current_dir) == NULL, true, "Errore getcwd!"); }
	memset(&read_n_response, 0, sizeof(server_response));
	init_request(&read_n_request, getpid(), READ_N, 0, NULL);
	read_n_request.files_to_read = N;
	if(N <= 0) N = 0;
	if(!good_path) puts(ANSI_COLOR_RED"Il path fornito non è assoluto: impossibile salvare i file!"ANSI_COLOR_RESET);
	handle_connection(read_n_request, &read_n_response);
	if(read_n_response.code[0] == FILE_NOT_EXISTS) return i;
	i++;
	if(good_path){
		CHECKERRNO(chdir(dirname) < 0, "Errore chdir");
		save_to_file(read_n_response.pathname, read_n_response.data, read_n_response.size);
		CHECKERRNO(chdir(current_dir) < 0, "Errore chdir");
	} 
	clean_response(&read_n_response);
	while(true){
		send_ack(socket_fd);
		if(read_all_buffer(socket_fd, &buffer, &buff_size) < 0) return -1;
		deserialize_response(&read_n_response, &buffer, buff_size);
		if(read_n_response.code[0] == FILE_NOT_EXISTS) break;
		if(good_path){
			CHECKERRNO(chdir(dirname) < 0, "Errore chdir");
			save_to_file(read_n_response.pathname, read_n_response.data, read_n_response.size);
			CHECKERRNO(chdir(current_dir) < 0, "Errore chdir");
		} 
		clean_response(&read_n_response);
		memset(&read_n_response, 0, sizeof read_n_response);
		i++;
	}
	clean_request(&read_n_request);
	clean_response(&read_n_response);
	return i;
}


int appendToFile(const char *pathname, void *buf, size_t size, const char *dirname){
	if(check_path(pathname, "appendToFile") < 0) return -1;
	client_request append_request;
	server_response append_response;
	unsigned char* buffer = NULL;
	size_t buff_size = 0;
	char current_dir[PATH_MAX] = {0};
	memset(&append_response, 0, sizeof(server_response));
	init_request(&append_request, getpid(), APPEND, 0, pathname);
	append_request.size = size;
	append_request.data = (unsigned char *)calloc(size, sizeof(unsigned char));
	memcpy(append_request.data, buf, size);
	handle_connection(append_request, &append_response);
	if(append_response.code[0] & FILE_OPERATION_FAILED){
		errno = check_error(append_response.code);
		clean_request(&append_request);
		clean_response(&append_response);
		return -1;
	}
	if(append_response.has_victim){
		while(true){
			send_ack(socket_fd);
			if(read_all_buffer(socket_fd, &buffer, &buff_size) < 0) return -1;
			deserialize_response(&append_response, &buffer, buff_size);
			if(append_response.size == 1 && append_response.code[0] == FILE_OPERATION_SUCCESS && append_response.data[0] == 0) break;
			if(dirname){
				CHECKERRNO(chdir(dirname) < 0, "Errore chdir");
				save_to_file(append_response.pathname, append_response.data, append_response.size);
				CHECKERRNO(chdir(current_dir) < 0, "Errore chdir");
			}
			clean_response(&append_response);
			memset(&append_response, 0, sizeof append_response);
		}
	}
	clean_request(&append_request);
	clean_response(&append_response);

	return 0;

}

int writeFile(const char* pathname, const char* dirname){
	if(check_path(pathname, "writeFile") < 0) return -1;
	struct stat file_info;
	int file;
	size_t size = 0, read_bytes = 0;
	client_request write_request;
	server_response write_response;
	unsigned char* buffer = NULL;
	size_t buff_size = 0;
	char current_dir[PATH_MAX] = {0};
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
	init_request(&write_request, getpid(), WRITE, 0, pathname);
	write_request.data = (unsigned char *) calloc(size, sizeof(unsigned char));
	CHECKALLOC(write_request.data, "Errore di allocazione writeFile");
	if((read_bytes = read(file, write_request.data, size)) != size)
		printf("read %lu, size %lu\n", read_bytes, size);
	write_request.size = size;
	handle_connection(write_request, &write_response);
	if(write_response.code[0] & FILE_OPERATION_FAILED){
		errno = check_error(write_response.code);
		clean_request(&write_request);
		clean_response(&write_response);
		return -1;
	}

	if(write_response.has_victim){
		while(true){
			send_ack(socket_fd);
			if(read_all_buffer(socket_fd, &buffer, &buff_size) < 0) return -1;
			deserialize_response(&write_response, &buffer, buff_size);
			if(write_response.size == 1 && write_response.code[0] == FILE_OPERATION_SUCCESS && write_response.data[0] == 0) break;
			if(dirname){
				CHECKERRNO(chdir(dirname) < 0, "Errore chdir");
				save_to_file(write_response.pathname, write_response.data, write_response.size);
				CHECKERRNO(chdir(current_dir) < 0, "Errore chdir");
			}
			clean_response(&write_response);
			memset(&write_response, 0, sizeof write_response);
		}
	}
	clean_request(&write_request);
	clean_response(&write_response);
	return 0;
}

int removeFile(const char* pathname){
	if(check_path(pathname, "removeFile") < 0) return -1;
	client_request remove_request;
	server_response remove_response;
	memset(&remove_response, 0, sizeof(server_response));
	init_request(&remove_request, getpid(), REMOVE, 0, pathname);
	handle_connection(remove_request, &remove_response);
	clean_request(&remove_request);
	if(remove_response.code[0] & FILE_OPERATION_FAILED){
		errno = check_error(remove_response.code);
		clean_response(&remove_response);
		return -1;
	}
	clean_response(&remove_response);
	return 0;
}


int lockFile(const char* pathname){
	if(check_path(pathname, "lockFile") < 0) return -1;
	client_request lock_request;
	server_response lock_response;
	memset(&lock_response, 0, sizeof(server_response));
	init_request(&lock_request, getpid(), SET_LOCK, O_LOCK, pathname);
	handle_connection(lock_request, &lock_response);
	clean_request(&lock_request);
	if(lock_response.code[0] & FILE_OPERATION_FAILED){
		errno = check_error(lock_response.code);
		clean_response(&lock_response);
		return -1;
	}
	clean_response(&lock_response);
	return 0;
}


int unlockFile(const char* pathname){
	if(check_path(pathname, "unlockFile") < 0) return -1;
	client_request unlock_request;
	server_response unlock_response;
	memset(&unlock_response, 0, sizeof(server_response));
	init_request(&unlock_request, getpid(), SET_LOCK, 0, pathname); // Se il flag e' O_LOCK fa il lock, altrimenti unlock
	handle_connection(unlock_request, &unlock_response);
	clean_request(&unlock_request);
	if(unlock_response.code[0] & FILE_OPERATION_FAILED){
		errno = check_error(unlock_response.code);
		clean_response(&unlock_response);
		return -1;
	}
	clean_response(&unlock_response);
	return 0;
}


ssize_t read_all_buffer(int com, unsigned char **buffer, size_t *buff_size){
	ssize_t read_bytes = 0;
	unsigned char packet_size_buff[sizeof(unsigned long)];
	memset(packet_size_buff, 0, sizeof(unsigned long));
	if (readn(com, packet_size_buff, sizeof packet_size_buff) < 0)
		return -1;
	*buff_size = char_to_ulong(packet_size_buff);
	if(!send_ack(com)) return -1;
	*buffer = realloc(*buffer, *buff_size);
	read_bytes = readn(com, *buffer, *buff_size);
	return read_bytes;
}

bool get_ack(int com){
	unsigned char acknowledge = 0;
	if(read(com, &acknowledge, 1) < 0) return false;
	return true;
}

int handle_connection(client_request request, server_response *response){
	unsigned char *buffer = NULL;
	unsigned long buff_size = 0;
	unsigned char packet_size_buff[sizeof(unsigned long)];
	serialize_request(request, &buffer, &buff_size);
	ulong_to_char(buff_size, packet_size_buff);
	writen(socket_fd, packet_size_buff, sizeof packet_size_buff);
	if(get_ack(socket_fd)){
		writen(socket_fd, buffer, buff_size);
		reset_buffer(&buffer, &buff_size);
		if(read_all_buffer(socket_fd, &buffer, &buff_size) < 0) return -1;
		deserialize_response(response, &buffer, buff_size);
		return 0;
	}
	free(buffer);
	return -1;
}

int mkpath(const char* pathname){
	char *tmpstr = NULL;
	char *token = NULL;
	char *path = (char *) calloc(strlen(pathname) + 1, sizeof(char));
	char original_dir[PATH_MAX] = {0};
	CHECKEXIT(getcwd(original_dir, sizeof original_dir), true, "Errore getcwd");
	strcpy(path, pathname);
	for (int i = strlen(path)-1; ; i--){
		if(path[i] == '/'){
			path[i] = 0;
			break;
		}
		path[i] = 0;
	}
	token = strtok_r(path, "/", &tmpstr);
	while(token){
		if(chdir(token) < 0){
			if(mkdir(token, 0777) == -1){
				perror(ANSI_COLOR_RED"Errore durante la creazione della cartella");
				puts(ANSI_COLOR_RESET);
				CHECKERRNO(chdir(original_dir) < 0, "Errore chdir");
				return -1;
			}
			CHECKERRNO(chdir(token) < 0, "Errore chdir");
		}
		token = strtok_r(NULL, "/", &tmpstr);
	}
	free(path);
	CHECKERRNO(chdir(original_dir) < 0, "Errore chdir");
	return 0;
}

int save_to_file(const char* pathname, unsigned char* data, size_t size){
	int fd = 0;
	char* path = (char *) calloc(strlen(pathname) + 1, sizeof(char));
	strcpy(path, (pathname[0] == '/') ? pathname + 1 : pathname);
	errno = 0;
	if((fd = open(path, O_CREAT | O_RDWR, 0777)) < 0){
		if (errno == ENOENT){
			mkpath(path);
			if((fd = open(path, O_CREAT | O_RDWR, 0777)) < 0){
				free(path);
				return -1;
			}
		}
		else{
			free(path);
			return -1;
		}
		
	}
	CHECKSCEXIT(write(fd, data, size), true, "Errore write save_to_file");
	close(fd);
	free(path);
	return 0;
}