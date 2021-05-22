#ifndef COMMON_INCLUDES_H
#define COMMON_INCLUDES_H
#include "common_includes.h"
#endif

#define OPEN 0x1
#define READ 0x02
#define WRITE 0x04
#define APPEND 0x08
#define O_CREATE 0x10
#define O_LOCK 0x20

#define FILE_OPEN_SUCCESS 0x01 
#define FILE_CLOSE_SUCCESS 0x02
#define FILE_CREATE_SUCCESS 0x04
#define FILE_LOCK_SUCCESS 0x08
#define FILE_WRITE_SUCCESS 0x10
#define FILE_READ_SUCCESS 0x20
#define FILE_UNLOCK_SUCCESS 0x40

#define FILE_OPEN_FAILED 0x01
#define FILE_CLOSE_FAILED 0x02
#define FILE_CREATE_FAILED 0x04
#define FILE_LOCK_FAILED 0x08
#define FILE_WRITE_FAILED 0x10
#define FILE_READ_FAILED 0x20
#define FILE_UNLOCK_SUCCESS 0x40
#define FILE_EXISTS 0x80


typedef struct client_request_{
	pid_t client_id;
	char *pathname;
	char *dirname;
	unsigned char *data;
	unsigned int size;
	unsigned char command;
} client_request;

typedef struct server_response_{
	char *filename;
	unsigned char *data;
	unsigned int size;
	unsigned char code[2];
	bool deleted_file;
} server_response;

