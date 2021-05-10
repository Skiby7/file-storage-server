#ifndef STD_H
#define STD_H
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#endif
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <pthread.h>
#include <time.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <signal.h>


#define UNIX_MAX_PATH 108

#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_GREEN   "\x1b[32m"
#define ANSI_COLOR_YELLOW  "\x1b[33m"
#define ANSI_COLOR_BLUE    "\x1b[34m"
#define ANSI_COLOR_MAGENTA "\x1b[35m"
#define ANSI_COLOR_CYAN    "\x1b[36m"
#define ANSI_COLOR_RESET   "\x1b[0m"
#define ANSI_CLEAR_SCREEN "\033[2J\033[H"

typedef struct pargs_{
	int socket_fd;
	unsigned short whoami;
}pargs;

typedef struct workers_{
	int tid;
	struct workers_ *next;
} workers;


void printconf();
void init(char* sockname);
void* conneciton_handler(void* com);
void* wait_workers(void* args);
void* refuse_connection(void* args);
int rand_r(unsigned int *seedp);
void signal_handler(int signum);

