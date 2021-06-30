#ifndef COMMON_INCLUDES_H
#define COMMON_INCLUDES_H
#include "common_includes.h"
#endif

#define DELIM ": "
#define MAX_BUFFER_LEN 151

typedef struct _config{
	int workers;
	int mem;
	int files;
	char *sockname;
	char *log;
	bool summary;
} config;


char *strtok_r(char *str, const char *delim, char **saveptr);
int parseConfig(FILE *conf, config *configuration);
void freeConfig(config *configuration);