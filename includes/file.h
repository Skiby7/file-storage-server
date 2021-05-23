#ifndef COMMON_INCLUDES_H
#define COMMON_INCLUDES_H
#include "common_includes.h"
#endif
#include <sys/stat.h>


typedef struct fssFile_{
	char *name;
	unsigned char *data;
	bool locked;
	bool deleted;
	unsigned short use_stat;
	unsigned int client_open_id;
	unsigned long size;
	time_t create_time;
	time_t last_modified;
	pthread_mutex_t file_mutex;
} fssFile;

typedef struct storage_{
	fssFile **storage_table;
	unsigned long size;
	unsigned long size_limit;
	unsigned long max_size_reached;
	unsigned int file_count;
	unsigned long file_limit;
	unsigned int max_file_num_reached;
} storage;

int init_storage(int max_file_num, int max_size);
unsigned int search_file(const char* pathname);
unsigned int get_free_index(const char* pathname);
int clean_storage();
int open_file(char *filename, int flags, int client_id, server_response *response);
int read_file(char *filename, server_response *response);
int write_to_file(unsigned char *data, int length, char *filename, int client_id, server_response *response);
int append_to_file(unsigned char* new_data, int new_data_size, char *filename, int client_id, server_response *response);
