#ifndef COMMON_INCLUDES_H
#define COMMON_INCLUDES_H
#include "common_includes.h"
#endif

typedef struct fssFile_{
	char *name;
	unsigned char *data;
	bool locked;
	int client_locking;
	unsigned long size;
	time_t create_time;
	time_t last_modified;
} fssFile;

typedef struct storage_{
	fssFile *storage_table;
	unsigned long size;
	unsigned long size_limit;
	unsigned long max_size_reached;
	unsigned int file_count;
	unsigned long file_limit;
	unsigned int max_file_num_reached;
} storage;

int init_storage(int max_file_num, int max_size);