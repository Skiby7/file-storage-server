#include <file.h>
#include "fssApi.h"



extern int socket_fd;
char open_connection_name[UNIX_MAX_PATH] = "None";
extern pthread_mutex_t storage_access_mtx;
extern storage server_storage;

ssize_t readn(int fd, void *ptr, size_t n) {  
   size_t   nleft;
   ssize_t  nread;
 
   nleft = n;
   while (nleft > 0) {
     if((nread = read(fd, ptr, nleft)) < 0) {
        if (nleft == n) return -1; /* error, return -1 */
        else break; /* error, return amount read so far */
     } else if (nread == 0) break; /* EOF */
     nleft -= nread;
     ptr   += nread;
   }
   return(n - nleft); /* return >= 0 */
}


ssize_t writen(int fd, void *ptr, size_t n) {  
   size_t   nleft;
   ssize_t  nwritten;
 
   nleft = n;
   while (nleft > 0) {
     if((nwritten = write(fd, ptr, nleft)) < 0) {
        if (nleft == n) return -1; /* error, return -1 */
        else break; /* error, return amount written so far */
     } else if (nwritten == 0) break; 
     nleft -= nwritten;
     ptr   += nwritten;
   }
   return(n - nleft); /* return >= 0 */
}
 

int openConnection(const char* sockname, int msec, const struct timespec abstime){
	int i = 1, connection_status = -1;
	struct sockaddr_un sockaddress;
	struct timespec wait_reconnect = {
		.tv_nsec = msec*1000000,
		.tv_sec = 0
	};
	struct timespec remaining_until_failure = {
		.tv_nsec = 0,
		.tv_sec = 0
	};
	if(strncmp(open_connection_name, "None", UNIX_MAX_PATH) != 0){
		errno = ENFILE;
		return -1;
	}
	memset(open_connection_name, 0, UNIX_MAX_PATH);
	strncpy(sockaddress.sun_path, sockname, UNIX_MAX_PATH);
	strncpy(open_connection_name, sockname, UNIX_MAX_PATH);
	sockaddress.sun_family = AF_UNIX;
	socket_fd = socket(AF_UNIX, SOCK_STREAM, 0);
	puts(ANSI_COLOR_CYAN"Provo a connettermi...\n"ANSI_COLOR_RESET);
	while((connection_status = connect(socket_fd,(struct sockaddr*) &sockaddress,sizeof(sockaddress))) == -1){
		nanosleep(&wait_reconnect, NULL);
		if(remaining_until_failure.tv_nsec + wait_reconnect.tv_nsec == 1e+9){
			remaining_until_failure.tv_nsec = 0;
			remaining_until_failure.tv_sec++;
		}
		else
			remaining_until_failure.tv_nsec += wait_reconnect.tv_nsec;
		printf(ANSI_COLOR_RED"\033[ATentativo %d fallito...\n"ANSI_COLOR_RESET, i++);
		if(remaining_until_failure.tv_nsec == abstime.tv_nsec && remaining_until_failure.tv_sec == abstime.tv_sec){
			printf(ANSI_COLOR_RED"Connection timeout\n"ANSI_COLOR_RESET);
			errno = ETIMEDOUT;
			return connection_status;
		}
	}
	return connection_status;
}

int closeConnection(const char* sockname){
	if(strncmp(sockname, open_connection_name, UNIX_MAX_PATH) != 0){
		errno = EINVAL;
		return -1;
	}
	memset(open_connection_name, 0, UNIX_MAX_PATH);
	strncpy(open_connection_name, "None", UNIX_MAX_PATH);
	return close(socket_fd);
}

int openFile(const char* pathname, int flags){
	int file;
	int mode = 0;
	int exists = 0;
	int create = O_CREATE && flags;
	if(access(pathname, F_OK) == 0)
		exists = 1;
	
	if(create && exists){
		errno = EEXIST;
		return -1;
	}
	if(!create && !exists){
		errno = EINVAL;
		return -1;
	}
		
	
	if((file = open(pathname, flags)) == -1){
		perror("Open File");
		exit(EXIT_FAILURE);
	}
	return 0;
}

int readFile(const char* pathname, void** buf, size_t* size){
	int file_index = 0;
	unsigned char* data = NULL;
	SAFELOCK(storage_access_mtx);
	file_index = search_file(pathname);
	SAFEUNLOCK(storage_access_mtx);
	if(file_index != -1){
		SAFELOCK(storage_access_mtx);
		*size = server_storage.storage_table[file_index].size;
		data = (unsigned char *) malloc(*size*sizeof(unsigned char));
		memcpy(data, server_storage.storage_table[file_index].data, *size); // check if returns NULL
		SAFEUNLOCK(storage_access_mtx);
		*buf = data;
		return 0;
	}
	errno = ENOENT;
	return -1;
}



