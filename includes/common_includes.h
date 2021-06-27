#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>
#include <sys/types.h>
#include <pthread.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <signal.h>
#include <limits.h>

// COMMANDS
#define OPEN 0x01
#define CLOSE 0x02
#define READ 0x04
#define WRITE 0x08
#define APPEND 0x10
#define REMOVE 0x20
#define QUIT 0x40
#define SET_LOCK 0x80

// FLAGS
#define O_CREATE 0x01
#define O_LOCK 0x02

// RESPONSE CODES
#define FILE_OPERATION_SUCCESS 0x01 
#define FILE_OPERATION_FAILED 0x02
#define FILE_ALREADY_OPEN 0x04
#define FILE_ALREADY_LOCKED 0x08
#define FILE_LOCKED_BY_OTHERS 0x10
#define FILE_NOT_LOCKED 0X20
#define FILE_EXISTS 0x40
#define FILE_NOT_EXISTS 0x80



#define UNIX_MAX_PATH 200
#define AF_UNIX_MAX_PATH 108
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

#ifdef PIPE_BUF
	#undef PIPE_BUF
	#define PIPE_BUF 20
#else
	#define PIPE_BUF 20
#endif


					
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
	if(condizione){							\
		if(printErrno){						\
			perror("Errore -> "msg);		\
			fprintf(stderr, "(file %s, linea %d)\n", __FILE__, __LINE__);			\
		}						\
		else						\
			fprintf(stderr, "Errore (file %s, linea %d): "msg"\n", __FILE__, __LINE__);	\
		exit(EXIT_FAILURE);				\
	}

#define CHECKSCEXIT(call, printErrno, msg)			\
	if(call < 0){							\
		if(printErrno){						\
			perror("Errore -> "msg);		\
			fprintf(stderr, "(file %s, linea %d)\n", __FILE__, __LINE__);			\
		}						\
		else						\
			fprintf(stderr, "Errore (file %s, linea %d): "msg"\n", __FILE__, __LINE__);	\
		exit(EXIT_FAILURE);				\
	}


#define CHECKERRNO(condizione, msg)	if(condizione) {perror("Errore -> "msg); fprintf(stderr, "(file %s, linea %d)\n", __FILE__, __LINE__); errno = 0;}

#define CHECKALLOC(pointer, msg) if(pointer == NULL) {fprintf(stderr, "Memoria esaurita (file %s, linea %d): "msg"\n", __FILE__, __LINE__);exit(EXIT_FAILURE);}
// #define CHECKPOLL(poll_val)	if(poll_val == -1) {if(errno == EINTR) continue; else {perror("Errore poll -> "); fprintf(stderr, "(file %s, linea %d)\n", __FILE__, __LINE__); fflush(stderr);}}


#define SAFELOCK(mutex_var)				\
	if(pthread_mutex_lock(&mutex_var) != 0){						\
		fprintf(stderr, "Errore (file %s, linea %d): lock di "#mutex_var" non riuscita\n", __FILE__, __LINE__);		\
		exit(EXIT_FAILURE);			\
	}

#define SAFEUNLOCK(mutex_var)				\
	if(pthread_mutex_unlock(&mutex_var) != 0){						\
		fprintf(stderr, "Errore (file %s, linea %d): unlock di "#mutex_var" non riuscita\n", __FILE__, __LINE__);		\
		exit(EXIT_FAILURE);			\
	}
	
#define CHECKRW(call, size, msg)			\
	if(call < 0){							\
		perror("Errore -> "msg);		\
		fprintf(stderr, "(file %s, linea %d)\n", __FILE__, __LINE__);			\
		exit(EXIT_FAILURE);				\
	} \
	else if(call != size){ \
		fprintf(stderr, "Errore, non tutti i bytes sono stati scritti -> "msg);		\
		fprintf(stderr, "(file %s, linea %d)\n", __FILE__, __LINE__);			\
		exit(EXIT_FAILURE);				\
	}
