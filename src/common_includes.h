#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <pthread.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <signal.h>
#include <limits.h>

#define UNIX_MAX_PATH 108
#define EALLOC "Error while allocating memory!"

#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_GREEN   "\x1b[32m"
#define ANSI_COLOR_YELLOW  "\x1b[33m"
#define ANSI_COLOR_BLUE    "\x1b[34m"
#define ANSI_COLOR_MAGENTA "\x1b[35m"
#define ANSI_COLOR_CYAN    "\x1b[36m"
#define ANSI_COLOR_RESET   "\x1b[0m"
#define ANSI_CLEAR_SCREEN "\033[2J\033[H"

#define CONF_LINE "├────────────────────────────────────┤\n"
#define CONF_LINE_TOP "┌────────────────────────────────────┐\n"
#define CONF_LINE_BOTTOM "└────────────────────────────────────┘\n"

					
#define WELCOME_MESSAGE "            __ _ _             _                             \n"\
"           / _(_) |           | |                            \n"\
"          | |_ _| | ___    ___| |_ ___  _ __ __ _  __ _  ___ \n"\
"          |  _| | |/ _ \\  / __| __/ _ \\| '__/ _` |/ _` |/ _ \\\n"\
"          | | | | |  __/  \\__ \\ || (_) | | | (_| | (_| |  __/\n"\
"          |_| |_|_|\\___|  |___/\\__\\___/|_|  \\__,_|\\__, |\\___|\n"\
"                                                  __/ |     \n"\
"                                                 |___/      \n"\
"                                                  \n"\
"                                                  \n"\
"                    ___  ___ _ ____   _____ _ __ \n"\
"                   / __|/ _ \\ '__\\ \\ / / _ \\ '__|\n"\
"                   \\__ \\  __/ |   \\ V /  __/ |   \n"\
"                   |___/\\___|_|    \\_/ \\___|_|   \n"\
"                                                  \n"\
"                                                  \n"\

#define PRINT_WELCOME printf(ANSI_CLEAR_SCREEN ANSI_COLOR_CYAN"%s"ANSI_COLOR_RESET, WELCOME_MESSAGE); 

#define PRINT_POLLING(increment) \
	switch(increment){ \
		case 0: \
		increment += 1; \
		printf(ANSI_COLOR_YELLOW"* "ANSI_COLOR_RESET);  \
		break; \
		case 1: \
		increment = 0; \
		printf(ANSI_COLOR_YELLOW" *"ANSI_COLOR_RESET);  \
		break; \
	}\
	printf("\033[2D");
	


#define CHECKEXIT(condizione, printErrno, msg)			\
	if(condizione)						\
	{							\
		if(printErrno)					\
		{						\
			perror("Errore -> "msg);		\
			fprintf(stderr, "(file %s, linea %d)\n", __FILE__, __LINE__);			\
		}						\
		else						\
			fprintf(stderr, "Errore (file %s, linea %d): "msg"\n", __FILE__, __LINE__);	\
		fflush(stderr);					\
		exit(EXIT_FAILURE);				\
	}

#define CHECKSCEXIT(call, printErrno, msg)			\
	if(call < 0)						\
	{							\
		if(printErrno)					\
		{						\
			perror("Errore -> "msg);		\
			fprintf(stderr, "(file %s, linea %d)\n", __FILE__, __LINE__);			\
		}						\
		else						\
			fprintf(stderr, "Errore (file %s, linea %d): "msg"\n", __FILE__, __LINE__);	\
		fflush(stderr);					\
		exit(EXIT_FAILURE);				\
	}


#define CHECKERRNO(condizione, msg)	if(condizione) {perror("Errore -> "msg); fprintf(stderr, "(file %s, linea %d)\n", __FILE__, __LINE__); fflush(stderr);}

#define CHECKALLOC(pointer, msg) if(pointer == NULL) {fprintf(stderr, "Memoria esaurita (file %s, linea %d): "msg"\n", __FILE__, __LINE__);fflush(stderr);exit(EXIT_FAILURE);}
// #define CHECKPOLL(poll_val)	if(poll_val == -1) {if(errno == EINTR) continue; else {perror("Errore poll -> "); fprintf(stderr, "(file %s, linea %d)\n", __FILE__, __LINE__); fflush(stderr);}}


#define SAFELOCK(mutex_var)				\
	if(pthread_mutex_lock(&mutex_var) != 0)		\
	{						\
		fprintf(stderr, "Errore (file %s, linea %d): lock del semaforo #mutex_var non riuscita\n", __FILE__, __LINE__);		\
		exit(EXIT_FAILURE);			\
	}

#define SAFEUNLOCK(mutex_var)				\
	if(pthread_mutex_unlock(&mutex_var) != 0)	\
	{						\
		fprintf(stderr, "Errore (file %s, linea %d): unlock del semaforo #mutex_var non riuscita\n", __FILE__, __LINE__);		\
		exit(EXIT_FAILURE);			\
	}
	