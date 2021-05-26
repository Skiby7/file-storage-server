#ifndef COMMON_INCLUDES_H
#define COMMON_INCLUDES_H
#include "common_includes.h"
#endif
#include <sys/stat.h>
#include "connections.h"

#define BITS_IN_int     ( sizeof(int) * CHAR_BIT )
#define THREE_QUARTERS  ((int) ((BITS_IN_int * 3) / 4))
#define ONE_EIGHTH      ((int) (BITS_IN_int / 8))
#define HIGH_BITS       ( ~((unsigned int)(~0) >> ONE_EIGHTH ))

typedef struct clients_{
	int id;
	struct clients_ *next;
} clients_file_queue;


typedef struct fssFile_{
	char *name;
	unsigned char *data;
	bool deleted;
	unsigned short use_stat;
	clients_file_queue *clients_open;
	unsigned int whos_locking;
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
void clean_storage();
int open_file(char *filename, int flags, int client_id, server_response *response);
int close_file(char *filename, int client_id, server_response *response);
int read_file(char *filename, unsigned char **buffer, int client_id, server_response *response);
int write_to_file(unsigned char *data, int length, char *filename, int client_id, server_response *response);
int append_to_file(unsigned char* new_data, int new_data_size, char *filename, int client_id, server_response *response);
int lock_file(char *filename, int client_id, server_response *response);
int unlock_file(char *filename, int client_id, server_response *response);
void print_storage();