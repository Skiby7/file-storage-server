#ifndef COMMON_INCLUDES_H
#define COMMON_INCLUDES_H
#include "common_includes.h"
#endif



#define DELIM ": "



typedef struct _config{
	int workers;
	int mem;
	int files;
	char *sockname;
	char *log;
} config;


char *strtok_r(char *str, const char *delim, char **saveptr);
int parseConfig(FILE *conf, config *configuration);
