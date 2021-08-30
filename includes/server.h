#ifndef COMMON_INCLUDES_H
#define COMMON_INCLUDES_H
#include "common_includes.h"
#endif
#include <poll.h>

#define WELCOME_MESSAGE  "\n _              __                        \n"\
						 "|_ o |  _      (_ _|_  _  ._ _.  _   _    \n"\
						 "|  | | (/_     __) |_ (_) | (_| (_| (/_   \n"\
						 "                                 _|       \n"\
						 "      __                                  \n"\
						 "     (_   _  ._    _  ._                  \n"\
						 "     __) (/_ | \\/ (/_ |                   \n\n"\
                            
						
#define PRINT_WELCOME printf(ANSI_CLEAR_SCREEN ANSI_COLOR_CYAN"%s"ANSI_COLOR_RESET, WELCOME_MESSAGE); 

void printconf();
void init(char* sockname, char* config_file);
void* connection_handler(void* com);
void* wait_workers(void* args);
void* refuse_connection(void* args);
int rand_r(unsigned int *seedp);
void signal_handler(int signum);
void insert_com_fd(int com, nfds_t *size, nfds_t *count, struct pollfd *com_fd);
nfds_t realloc_com_fd(struct pollfd **com_fd, nfds_t free_slot);
void* sig_wait_thread(void *args);
