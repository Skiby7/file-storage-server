#include "file.h"
#include "hash.h"
#include "connections.h"


/** TODO:
 * - Controllare il numero di malloc e free
 * - Dare a ogni file una mutex in modo che possa fare la copia in e out senza bloccare nessuno
 * - Controllare l'accesso ai campi della struct e alla storage table
 * - Check del filecount in get_free_index
*/

storage server_storage;
pthread_mutex_t storage_access_mtx = PTHREAD_MUTEX_INITIALIZER;

/**
 * Check whether there's enough space in memory or not
 * 
 * @param new_size size of the file to be inserted
 * @param old_size size of the file to be replaced. 0 if the new file does not overwrite anything
 * 
 * @returns 0 if the operation is successful, -1 if the file is too big or there are no locked files to remove
 *
 */
static int check_memory(unsigned int new_size, unsigned int old_size){
	int max_capacity = 0, actual_capacity = 0, table_size = 0, clean_level = 0;
	SAFELOCK(storage_access_mtx);
	max_capacity = server_storage.size_limit;
	actual_capacity = server_storage.size_limit;
	table_size = server_storage.file_limit;
	if(new_size > max_capacity){
		SAFEUNLOCK(storage_access_mtx);
		return -1;
	}
	while(new_size + (actual_capacity - old_size) > max_capacity){
		for(int i = 0; i < table_size; i++){
			if(server_storage.storage_table[i] != NULL){
				if(pthread_mutex_trylock(&server_storage.storage_table[i]->file_mutex) == 0){ // Se non avviene il lock vuole dire che il file e' in uso
					if(server_storage.storage_table[i]->use_stat == clean_level && !server_storage.storage_table[i]->locked){
						server_storage.storage_table[i]->deleted = true;
						SAFELOCK(storage_access_mtx);
						server_storage.file_count -= 1;
						server_storage.size -= server_storage.storage_table[i]->size;
						SAFEUNLOCK(storage_access_mtx);
						server_storage.storage_table[i]->size = 0;
						free(server_storage.storage_table[i]->data);
					}
					SAFEUNLOCK(server_storage.storage_table[i]->file_mutex);
					if(new_size + (actual_capacity - old_size) > max_capacity) break;
				}
			}
		}
		if(clean_level == 2){
			SAFEUNLOCK(storage_access_mtx);
			return -1;
		}
		clean_level++;
	}
	SAFEUNLOCK(storage_access_mtx);
	return 0;
}

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

int open_file(char *filename, int flags, int client_id, server_response *response){
	int file_index = search_file(filename);
	bool file_exists = false;
	bool create_file = false;
	bool lock_file = false;
	if(file_index != -1) file_exists = true;
	if(flags & O_CREATE) create_file = true;
	if(flags & O_LOCK) lock_file = true;

	if(file_exists && create_file){
		response->code[0] = FILE_EXISTS;
		return -1;
	}
	if(!file_exists && !create_file){
		response->code[1] = FILE_OPEN_FAILED;
		return -1;
	}
	if(!file_exists){
		init_file(client_id, filename, lock_file);
	}
	if(file_exists){
		SAFELOCK(server_storage.storage_table[file_index]->file_mutex);
		server_storage.storage_table[file_index]->client_open_id = client_id;
		server_storage.storage_table[file_index]->locked = lock_file;
		SAFEUNLOCK(server_storage.storage_table[file_index]->file_mutex);
	}
	response->code[0] = FILE_OPEN_SUCCESS;
	return 0;
}




int write_to_file(unsigned char *data, int length, char *filename, int client_id, server_response *response){
	int file_index = search_file(filename);
	int old_size = 0;
	if(file_index == -1){
		response->code[1] = FILE_WRITE_FAILED | FILE_NOT_EXISTS;
		return -1;
	}
	SAFELOCK(server_storage.storage_table[file_index]->file_mutex);
	if(server_storage.storage_table[file_index]->client_open_id == client_id && server_storage.storage_table[file_index]->locked){
		old_size = server_storage.storage_table[file_index]->size;
		SAFEUNLOCK(server_storage.storage_table[file_index]->file_mutex);
		if(check_memory(length, old_size) < 0){
			response->code[2] = EFBIG;
			response->code[1] = FILE_WRITE_FAILED;
			return -1;
		}
		SAFELOCK(storage_access_mtx);
		server_storage.size += length;
		SAFEUNLOCK(storage_access_mtx);


		SAFELOCK(server_storage.storage_table[file_index]->file_mutex);
		server_storage.storage_table[file_index] = (unsigned char *) realloc(server_storage.storage_table[file_index], length);
		CHECKALLOC(server_storage.storage_table[file_index], "Errore allocazione wite_to_file");
		memcpy(server_storage.storage_table[file_index], data, length);
		server_storage.storage_table[file_index]->size = length;
		server_storage.storage_table[file_index]->last_modified = time(NULL);
		SAFEUNLOCK(server_storage.storage_table[file_index]->file_mutex);
		
		response->code[0] = FILE_WRITE_SUCCESS;
		return 0;
	}
	SAFEUNLOCK(server_storage.storage_table[file_index]->file_mutex);
	response->code[1] = FILE_WRITE_FAILED;
	return -1;
}

int append_to_file(unsigned char* new_data, int new_data_size, char *filename, int client_id, server_response *response){
	int file_index = search_file(filename);
	int old_size = 0;
	if(file_index == -1){
		response->code[1] = FILE_WRITE_FAILED | FILE_NOT_EXISTS;
		return -1;
	}
	SAFELOCK(server_storage.storage_table[file_index]->file_mutex);
	if(!server_storage.storage_table[file_index]->locked){
		old_size = server_storage.storage_table[file_index]->size;
		SAFEUNLOCK(server_storage.storage_table[file_index]->file_mutex);
		if(check_memory(new_data_size + old_size, 0) < 0){
			response->code[2] = EFBIG;
			response->code[1] = FILE_WRITE_FAILED;
			return -1;
		}
		SAFELOCK(storage_access_mtx);
		server_storage.size += new_data_size;
		SAFEUNLOCK(storage_access_mtx);
		SAFELOCK(server_storage.storage_table[file_index]->file_mutex);
		server_storage.storage_table[file_index] = (unsigned char *) realloc(server_storage.storage_table[file_index], new_data_size + old_size);
		CHECKALLOC(server_storage.storage_table[file_index], "Errore allocazione append_to_file");
		for (size_t i = old_size, j = 0; j < new_data_size; i++, j++)
			server_storage.storage_table[file_index]->data[i] = new_data[j];

		server_storage.storage_table[file_index]->size = new_data_size + old_size;
		server_storage.storage_table[file_index]->last_modified = time(NULL);
		SAFEUNLOCK(server_storage.storage_table[file_index]->file_mutex);
		response->code[0] = FILE_WRITE_SUCCESS;
		return 0;
	}
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

/**
 * Search for an unused index in the hash table
 * 
 * @param pathname the file to be inserted
 * 
 * @returns the first free index
 *
 */

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

void init_file(int id, char *filename, bool locked){
	int index = get_free_index(filename);
	SAFELOCK(storage_access_mtx);
	server_storage.file_count += 1;
	if(server_storage.file_count > server_storage.max_file_num_reached)
		server_storage.max_file_num_reached += 1;
	server_storage.storage_table[index] = (fssFile *) malloc(sizeof(fssFile));
	CHECKALLOC(server_storage.storage_table[index], "Errore inserimento nuovo file");
	memset(server_storage.storage_table[index], 0, sizeof(fssFile));
	server_storage.storage_table[index]->client_open_id = id;
	server_storage.storage_table[index]->create_time = time(NULL);
	server_storage.storage_table[index]->last_modified = time(NULL);
	if(pthread_mutex_init(&server_storage.storage_table[index]->file_mutex, NULL) != 0){
		fprintf(stderr, "Errore di inizializzazione file_mutex\n");
		exit(EXIT_FAILURE);
	}
	server_storage.storage_table[index]->deleted = false;
	server_storage.storage_table[index]->locked = locked;
	server_storage.storage_table[index]->name = (char *)calloc(strlen(filename) + 1, sizeof(char));
	CHECKALLOC(server_storage.storage_table[index]->name, "Errore inserimento nuovo file");
	strncpy(server_storage.storage_table[index]->name, filename, strlen(filename));
	server_storage.storage_table[index]->use_stat = 2;
	SAFEUNLOCK(storage_access_mtx);
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