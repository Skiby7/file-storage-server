#ifndef COMMON_INCLUDES_H
#define COMMON_INCLUDES_H
#include "common_includes.h"
#endif


typedef struct client_request_{
	uint32_t client_id;
	unsigned char command;
	unsigned char flags;
	int32_t files_to_read; // Serialized to 0xff 0xff 0xff 0xff if files_to_read <= 0
	uint32_t pathlen; // PATHLEN INCLUDES THE END CHARACTER
	char *pathname;
	uint64_t size;
	unsigned char* data;
} client_request;

typedef struct server_response_{
	uint32_t pathlen; // PATHLEN INCLUDES THE END CHARACTER
	char *pathname;
	unsigned char has_victim;
	unsigned char code[2]; // 1 -> RESULT 2 -> ERRNO
	uint64_t size;
	unsigned char* data;
} server_response;

typedef struct victim_queue_{
	server_response victim;
	struct victim_queue_ *next;
} victim_queue;
