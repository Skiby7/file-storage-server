#ifndef COMMON_INCLUDES_H
#define COMMON_INCLUDES_H
#include "common_includes.h"
#endif
#include <sys/stat.h>
#ifndef CONNECTIONS_H_
#define CONNECTIONS_H_
#include "connections.h"
#endif

// HASH_PJW MACRO
#define BITS_IN_int     ( sizeof(int) * CHAR_BIT )
#define THREE_QUARTERS  ((int) ((BITS_IN_int * 3) / 4))
#define ONE_EIGHTH      ((int) (BITS_IN_int / 8))
#define HIGH_BITS       ( ~((unsigned int)(~0) >> ONE_EIGHTH ))
#define TIME_BEFORE_UNLOCK 2 // In seconds

typedef struct clients_{
	int id;
	struct clients_ *next;
} open_file_client_list;

typedef struct waiting_lock{
	int id;
	int com;
	struct waiting_lock *next;
} lock_file_queue;

typedef struct fss_file{
	char *name;
	unsigned char *data;
	unsigned short use_stat;
	open_file_client_list *clients_open;
	lock_file_queue *waiting_lock;
	int whos_locking;
	uint64_t size;
	uint64_t uncompressed_size;
	unsigned short writers;
	unsigned int readers;
	time_t created_time;
	time_t last_access;
	pthread_mutex_t last_access_mtx;
	pthread_mutex_t order_mutex;
	pthread_mutex_t access_mutex;
	pthread_cond_t go_cond;
	struct fss_file *next;
} fss_file_t;

typedef struct storage_{
	fss_file_t **storage_table;	
	bool compression;
	unsigned char replacement_algo;
	unsigned short compression_level;
	unsigned int file_count;
	unsigned int file_limit;
	unsigned int max_file_num_reached;
	unsigned int total_evictions;
	unsigned int table_size;
	unsigned long size;
	unsigned long size_limit;
	unsigned long max_size_reached;
	pthread_mutex_t storage_access_mtx;
} fss_storage_t;

typedef struct victim_{
	char* pathname;
	unsigned short use_stat;
	uint64_t size;
	time_t last_access;
	time_t created_time;
} victim_t;

void init_table(int max_file_num, int max_size, bool compression, unsigned short compression_level, unsigned char replacement_algo);
void clean_storage();
int open_file(char *filename, int flags, int client_id, server_response *response);
int close_file(char *filename, int client_id, server_response *response);
int read_file(char *filename, int client_id, server_response *response);
int read_n_file(char **last_file, int client_id, server_response* response);
int write_to_file(unsigned char *data, int length, char *filename, int client_id, server_response *response, victim_queue** victims);
int append_to_file(unsigned char* new_data, int new_data_size, char *filename, int client_id, server_response *response, victim_queue** victims);
int remove_file(char *filename, int client_id,  server_response *response);
int lock_file(char *filename, int client_id, bool server_mutex, bool mutex_write, server_response *response);
int unlock_file(char *filename, int client_id, server_response *response);
int insert_lock_file_list(char *filename, int id, int com);
int pop_lock_file_list(char *filename, int *id, int *com, bool server_mutex, bool file_mutex);
void print_storage();
char* print_storage_info();
void print_summary();
void* use_stat_update(void *args);