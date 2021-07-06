#ifndef COMMON_INCLUDES_H
#define COMMON_INCLUDES_H
#include "common_includes.h"
#endif
#ifndef CONNECTIONS_H_
#define CONNECTIONS_H_
#include "connections.h"
#endif
#define BITS_IN_int     ( sizeof(int) * CHAR_BIT )
#define THREE_QUARTERS  ((int) ((BITS_IN_int * 3) / 4))
#define ONE_EIGHTH      ((int) (BITS_IN_int / 8))
#define HIGH_BITS       ( ~((unsigned int)(~0) >> ONE_EIGHTH ))

typedef struct clients_{
	int id;
	struct clients_ *next;
} open_file_client_list;

typedef struct lockers_{
	int id;
	int com;
	struct lockers_ *next;
} lock_file_queue;

typedef struct fssFile_{
	char *name;
	unsigned char *data;
	unsigned short use_stat;
	open_file_client_list *clients_open;
	lock_file_queue *lock_waiters;
	unsigned int whos_locking;
	unsigned long size;
	unsigned short writers;
	unsigned int readers;
	time_t create_time;
	time_t last_modified;
	pthread_mutex_t order_mutex;
	pthread_mutex_t access_mutex;
	pthread_cond_t go_cond;
	struct fssFile_ *next;
} fssFile;

typedef struct storage_{
	fssFile **storage_table;
	unsigned long size;
	unsigned long size_limit;
	unsigned long max_size_reached;
	unsigned int file_count;
	unsigned int file_limit;
	unsigned int max_file_num_reached;
	unsigned int total_evictions;
	pthread_mutex_t storage_access_mtx;
} storage;

typedef struct victim_{
	char* pathname;
	unsigned short use_stat;
	unsigned long size;
	time_t create_time;
	time_t last_modified;
} victim;

storage server_storage;

void init_table(int max_file_num, int max_size);
int check_memory(unsigned long new_size, char* caller);
int check_count();
int create_new_entry(int id, char *filename, bool locked);
int write_entry(fssFile entry, unsigned char op);