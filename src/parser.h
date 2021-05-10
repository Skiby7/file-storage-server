#ifndef STD_H
#define STD_H
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


char *strtok_r(char *str, const char *delim, char **saveptr);
int parseConfig(FILE *conf, config *configuration);
