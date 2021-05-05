#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define MAX_BUFFER_LEN 151
#define DELIM ": "

#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_GREEN   "\x1b[32m"
#define ANSI_COLOR_YELLOW  "\x1b[33m"
#define ANSI_COLOR_BLUE    "\x1b[34m"
#define ANSI_COLOR_MAGENTA "\x1b[35m"
#define ANSI_COLOR_CYAN    "\x1b[36m"
#define ANSI_COLOR_RESET   "\x1b[0m"
#define CLEAR_SCREEN_ANSI "\e[1;1H\e[2J"

typedef struct _config{
	int workers;
	int mem;
	int files;
	char *sockname;
	char *log;
} config;



int parseConfig(FILE *conf, config *configuration);
