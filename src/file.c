#include "file.h"
#include "hash.h"


/** TODO:
 * - Controllare il numero di malloc e free
 * - Dare a ogni file una mutex in modo che possa fare la copia in e out senza bloccare nessuno
 * - Controllare l'accesso ai campi della struct e alla storage table
*/

storage server_storage;
pthread_mutex_t storage_access_mtx = PTHREAD_MUTEX_INITIALIZER;

int init_storage(int max_file_num, int max_size){
	server_storage.storage_table = (fssFile **) malloc(2*max_file_num*sizeof(fssFile *));
	memset(server_storage.storage_table, 0, 2*max_file_num*sizeof(fssFile *));
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
		.deleted = false,
		.use_stat = 2
		
	};
	CHECKALLOC(buffer.name, "Errore durante la scrittura del file");
	CHECKALLOC(buffer.data, "Errore durante la scrittura del file");
	memccpy(buffer.data, data, length, sizeof(unsigned char));
}

int append_to_file(char *filename, unsigned char* newdata, int newdata_len){
	pthread_mutex_lock(&storage_access_mtx);
}

int clean_storage(){
	for(int i = 0; i < server_storage.file_limit; i++){
		if(server_storage.storage_table[i] != NULL){
			free(server_storage.storage_table[i]);
		}
	}
	free(server_storage.storage_table);
}

/**
 * Search for an entry in the hash table
 * 
 * @param pathname the file to be searched
 * 
 * @returns the index or -1 in case of file not found 
 *
 */
unsigned int search_file(const char* pathname){
	int i = 0, max_len = server_storage.file_limit, index = 0;
	unsigned int path_len = strlen(pathname);
	
	while(true){
		index =  hash_val(pathname, i, max_len, path_len);
		SAFELOCK(storage_access_mtx);
		if(server_storage.storage_table[index] == NULL){
			SAFEUNLOCK(storage_access_mtx);
			return -1; // File not found
		}
		SAFEUNLOCK(storage_access_mtx);

		SAFELOCK(server_storage.storage_table[index]->file_mutex);
		if(server_storage.storage_table[index]->deleted && strcmp(server_storage.storage_table[index]->name, pathname) == 0){
			SAFEUNLOCK(server_storage.storage_table[index]->file_mutex);
			return -1; // File eliminato
		}
		else if(strcmp(server_storage.storage_table[index]->name, pathname) == 0){
			SAFEUNLOCK(server_storage.storage_table[index]->file_mutex);
			return index; // Ho trovato il file
		}
		SAFEUNLOCK(server_storage.storage_table[index]->file_mutex);
			i++; // Non e' lui, vado avanti
	}
		
}

unsigned int get_free_index(const char* pathname){
	int i = 0, max_len = server_storage.file_limit, index = 0;
	unsigned int path_len = strlen(pathname);
	
	while(true){
		index =  hash_val(pathname, i, max_len, path_len);
		SAFELOCK(storage_access_mtx);
		if(server_storage.storage_table[index] == NULL){
			SAFEUNLOCK(storage_access_mtx);
			return index; // Il primo hash e' libero
		}
		SAFEUNLOCK(storage_access_mtx);

		SAFELOCK(server_storage.storage_table[index]->file_mutex);
		if(server_storage.storage_table[index]->deleted){
			SAFEUNLOCK(server_storage.storage_table[index]->file_mutex);
			return index; // Sovrascrivo una vecchia cella
		}
		SAFEUNLOCK(server_storage.storage_table[index]->file_mutex);
		i++; // Non e' un posto libero, vado avanti
	}
		
}


void* clock_algorithm(void *args){ // Lui va a diritto
	int table_size = 0;
	SAFELOCK(storage_access_mtx);
	table_size = server_storage.file_limit;
	SAFEUNLOCK(storage_access_mtx);
	while(true){
		for(int i = 0; i < table_size; i++){
			SAFELOCK(storage_access_mtx);
			if(server_storage.storage_table[i] == NULL){
				SAFEUNLOCK(storage_access_mtx);
				continue;
			}
			SAFEUNLOCK(storage_access_mtx);

			if(pthread_mutex_trylock(&server_storage.storage_table[i]->file_mutex) == 0){ // Se non avviene il lock vuole dire che il file e' in uso
				if(server_storage.storage_table[i]->use_stat != 0)
					server_storage.storage_table[i]->use_stat -= 1;
				SAFEUNLOCK(server_storage.storage_table[i]->file_mutex);
			}
		}
		sleep(5); // Provvisorio
	}
}