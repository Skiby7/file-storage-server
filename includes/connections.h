#ifndef COMMON_INCLUDES_H
#define COMMON_INCLUDES_H
#include "common_includes.h"
#endif


typedef struct client_request_{
	unsigned int client_id;
	unsigned char command;
	unsigned char flags;
	int files_to_read; // Serialized to 0xff 0xff 0xff 0xff
	unsigned int pathlen; // PATHLEN INCLUDES THE END CHARACTER
	char *pathname;
	unsigned long size;
	unsigned char* data;
}client_request;

typedef struct server_response_{
	unsigned int pathlen; // PATHLEN INCLUDES THE END CHARACTER
	char *pathname;
	unsigned char has_victim;
	unsigned char code[2]; // 1 -> RESULT 2 -> ERRNO
	unsigned long size;
	unsigned char* data;
} server_response;

typedef struct victim_queue_{
	server_response victim;
	struct victim_queue_ *next;
} victim_queue;



