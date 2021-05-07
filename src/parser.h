#ifndef STD_
#define STD_
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#endif



#define DELIM ": "
#define MAX_BUFFER_LEN 151


typedef struct _config{
	int workers;
	int mem;
	int files;
	char *sockname;
	char *log;
} config;



int parseConfig(FILE *conf, config *configuration);
