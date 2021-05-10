#ifndef COMMON_INCLUDES_H
#define COMMON_INCLUDES_H
#include "common_includes.h"
#endif

typedef struct fssFile_{
	unsigned char *data;
	bool locked;
	int client_locking;
	time_t create_time;
	time_t last_modified;
} fssFile;

typedef struct storage_{
	unsigned char **storage_table;
	unsigned long size;
	unsigned long size_limit;
	unsigned long max_size_reached;
	unsigned int file_count;
	unsigned long file_limit;
	unsigned int max_file_num_reached;
} storage;