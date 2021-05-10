#ifndef COMMON_INCLUDES_H
#define COMMON_INCLUDES_H
#include "common_includes.h"
#endif


typedef struct pargs_{
	int socket_fd;
	unsigned int whoami;
}pargs;

typedef struct connections_{
	int com;
	struct connections_ *next;
} connection;

void printconf();
void init(char* sockname);
void* connection_handler(void* com);
void* wait_workers(void* args);
void* refuse_connection(void* args);
int rand_r(unsigned int *seedp);
void signal_handler(int signum);

