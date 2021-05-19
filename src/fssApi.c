#include "fssApi.h"

extern int socket_fd;
char open_connection_name[UNIX_MAX_PATH] = "None";

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



