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
	CHECKALLOC(buffer.name, "Errore durante la scrittura del file");
	CHECKALLOC(buffer.data, "Errore durante la scrittura del file");
	memccpy(buffer.data, data, length, sizeof(unsigned char));
}

int append_to_file(char *filename, unsigned char* newdata, int newdata_len){
	pthread_mutex_lock(&storage_access);
}

int clean_storage(){
	for(int i = 0; i < server_storage.file_limit; i++){
		if(server_storage.storage_table[i].size != 0){
			free(server_storage.storage_table[i].name);
			free(server_storage.storage_table[i].data);
		}
	}
	free(server_storage.storage_table);
}

unsigned int hash_pjw(void* key){
    char *datum = (char *)key;
    unsigned int hash_value, i;

    if(!datum) return 0;

    for (hash_value = 0; *datum; ++datum) {
        hash_value = (hash_value << ONE_EIGHTH) + *datum;
        if ((i = hash_value & HIGH_BITS) != 0)
            hash_value = (hash_value ^ (i >> THREE_QUARTERS)) & ~HIGH_BITS;
    }
    return (hash_value);
}

static inline unsigned int hash_scan(void* key, unsigned int i, unsigned int max_len){
	return ((hash_pjw(key) + (i/2) + ((i*i)/2))%max_len);

}

unsigned int search_file(char* pathname){
	int i = 0, max_len = server_storage.file_limit;
	while(strcmp(server_storage.storage_table[hash_scan(pathname, i, max_len)].name, pathname) != 0) i++;
	
}