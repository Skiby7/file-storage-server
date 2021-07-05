#ifndef COMMON_INCLUDES_H
#define COMMON_INCLUDES_H
#include "common_includes.h"
#endif
#include <sys/stat.h>
#ifndef CONNECTIONS_H_
#define CONNECTIONS_H_
#include "connections.h"
#endif
#include "storage_table.h"


int init_storage(int max_file_num, int max_size);
void clean_storage();
int open_file(char *filename, int flags, int client_id, server_response *response);
int close_file(char *filename, int client_id, server_response *response);
int read_file(char *filename, int client_id, server_response *response);
int read_n_file(int *last_index, int client_id, server_response* response);
int write_to_file(unsigned char *data, int length, char *filename, int client_id, server_response *response);
int append_to_file(unsigned char* new_data, int new_data_size, char *filename, int client_id, server_response *response);
int remove_file(char *filename, int client_id,  server_response *response);
int lock_file(char *filename, int client_id, bool mutex_write, server_response *response);
int unlock_file(char *filename, int client_id, server_response *response);
int insert_lock_file_list(char *filename, int id, int com);
int pop_lock_file_list(char *filename, int *id, int *com);
void print_storage();
void print_storage_info();
void print_summary();
void* use_stat_update(void *args);