#ifndef COMMON_INCLUDES_H
#define COMMON_INCLUDES_H
#include "common_includes.h"
#include <poll.h>
#endif


typedef struct pargs_{
	int socket_fd;
	unsigned int whoami;
}pargs;

typedef struct connections_{
	int com;
	struct connections_ *next;
	struct connections_ *prev;
} ready_clients;

void printconf();
void init(char* sockname);
void* connection_handler(void* com);
void* wait_workers(void* args);
void* refuse_connection(void* args);
int rand_r(unsigned int *seedp);
void signal_handler(int signum);
static void insert_com_fd(int com, nfds_t *size, nfds_t *count, struct pollfd *com_fd);
static nfds_t realloc_com_fd(struct pollfd *com_fd, nfds_t free_slot);
static void insert_client_ready_list(int com, ready_clients **head, ready_clients **tail);
static int pop_client(ready_clients **tail);
static void clean_list(ready_clients **head);

