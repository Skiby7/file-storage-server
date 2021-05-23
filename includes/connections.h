#ifndef COMMON_INCLUDES_H
#define COMMON_INCLUDES_H
#include "common_includes.h"
#endif


typedef struct client_request_{
	pid_t client_id;
	char *pathname;
	unsigned char *data;
	unsigned int size;
	unsigned char command;
	unsigned char flags; 
} client_request;

typedef struct server_response_{
	char *filename;
	unsigned char *data;
	unsigned int size;
	unsigned char code[2]; // 1 -> RESULT 2 -> ERRNO
	bool deleted_file;
} server_response;

