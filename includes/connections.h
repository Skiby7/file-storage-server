#ifndef COMMON_INCLUDES_H
#define COMMON_INCLUDES_H
#include "common_includes.h"
#endif


typedef struct client_request_{
	unsigned int client_id;
	unsigned char command;
	unsigned char flags;
	char pathname[UNIX_MAX_PATH + 1];
	unsigned long size;
	unsigned char* data;
}client_request;

typedef struct server_response_{
	char filename[UNIX_MAX_PATH + 1];
	unsigned char code[2]; // 1 -> RESULT 2 -> ERRNO
	unsigned long size;
	unsigned char* data;
} server_response;



