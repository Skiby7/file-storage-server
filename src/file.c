#include "file.h"


// TO REMEMBER:
// - Check #mallocs == #frees

storage server_storage;
static pthread_mutex_t storage_access = PTHREAD_MUTEX_INITIALIZER;

int init_storage(int max_file_num, int max_size){
	
	server_storage.storage_table = (fssFile *) malloc(2*max_file_num*sizeof(fssFile));
	memset(server_storage.storage_table, 0, 2*max_file_num*sizeof(fssFile));
	server_storage.file_limit = max_file_num;
	server_storage.size_limit = max_size;
	server_storage.size = 0;
	server_storage.max_size_reached = 0;
	server_storage.max_file_num_reached = 0;
	server_storage.file_count = 0;
}

void print_storage(){

}

int write_to_file(unsigned char *data, int length, char *pathname, bool is_locked, int who_locks){
	fssFile buffer = {
		.create_time = time(NULL),
		.last_modified = time(NULL),
		.locked = is_locked,
		.client_locking = who_locks,
		.size = length,
	};
	if((buffer.name = (char *) malloc((strlen(pathname) + 1)*sizeof(char)) == NULL)) puts(EALLOC);
		

	if((buffer.data = (unsigned char*) malloc(length*sizeof(unsigned char*)) == NULL)) puts(EALLOC);

		
}