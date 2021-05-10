#include "file.h"


storage server_storage;
static pthread_mutex_t storage_access = PTHREAD_MUTEX_INITIALIZER;

int init_storage(int max_file_num, int max_size){
	server_storage.storage_table = (unsigned char *) malloc(2*max_file_num*sizeof(char *));
	server_storage.file_limit = max_file_num;
	server_storage.size_limit = max_size;
	server_storage.size = 0;
	server_storage.max_size_reached = 0;
	server_storage.max_file_num_reached = 0;
	server_storage.file_count = 0;
}

void print_storage(){




}