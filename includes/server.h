#ifndef COMMON_INCLUDES_H
#define COMMON_INCLUDES_H
#include "common_includes.h"
#endif
#include <poll.h>


void printconf();
void init(char* sockname);
void* connection_handler(void* com);
void* wait_workers(void* args);
void* refuse_connection(void* args);
int rand_r(unsigned int *seedp);
void signal_handler(int signum);
void insert_com_fd(int com, nfds_t *size, nfds_t *count, struct pollfd *com_fd);
nfds_t realloc_com_fd(struct pollfd **com_fd, nfds_t free_slot);
void* sig_wait_thread(void *args);
