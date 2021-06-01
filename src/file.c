#include "file.h"



/** TODO:
 * - Controllare il numero di malloc e free
 * - Check del filecount in get_free_index
*/

storage server_storage;
pthread_mutex_t storage_access_mtx = PTHREAD_MUTEX_INITIALIZER;
static void init_file(int id, char *filename, bool locked);
static unsigned int search_file(const char* pathname);
static unsigned int get_free_index(const char* pathname);
static unsigned int hash_val(const void* key, unsigned int i, unsigned int max_len, unsigned int key_len);
void start_read(int file_index);
void start_write(int file_index);
void stop_read(int file_index);
void stop_write(int file_index);

/**
 * Check whether there's enough space in memory or not
 * 
 * @param new_size size of the file to be inserted
 * @param old_size size of the file to be replaced. 0 if the new file does not overwrite anything
 * 
 * @returns 0 if the operation is successful, -1 if the file is too big or there are no locked files to remove
 *
 */
static int check_memory(unsigned int new_size){
	int max_capacity = 0, actual_capacity = 0, table_size = 0, clean_level = 0;
	SAFELOCK(storage_access_mtx);
	max_capacity = server_storage.size_limit;
	actual_capacity = server_storage.size;
	table_size = 2*server_storage.file_limit;
	if(new_size > max_capacity){
		SAFEUNLOCK(storage_access_mtx);
		return -1;
	}
	while(new_size + actual_capacity > max_capacity){
		for(int i = 0; i < table_size; i++){
			if(server_storage.storage_table[i] != NULL){
				start_write(i);
				if(server_storage.storage_table[i]->use_stat == clean_level && server_storage.storage_table[i]->whos_locking == -1){
					server_storage.storage_table[i]->deleted = true;
					server_storage.storage_table[i]->size = 0;
					free(server_storage.storage_table[i]->data);
					server_storage.file_count -= 1;
					server_storage.size -= server_storage.storage_table[i]->size;
				}
				stop_write(i);
				if(new_size + actual_capacity > max_capacity) break;
			
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
	return 0;
}

void print_storage(){
	for (size_t i = 0; i < 2*server_storage.file_limit; i++)
	{
		if(server_storage.storage_table[i] != NULL){
			puts(server_storage.storage_table[i]->name);
			printf("size: %d", server_storage.storage_table[i]->size);
			if(server_storage.storage_table[i]->data != NULL){
				for (size_t j = 0; j < server_storage.storage_table[i]->size; j++)
				{
					printf("%c", server_storage.storage_table[i]->data[j]);
				}
				
			}
		}
	}
	

}

static int check_client_id(clients_file_queue *head, int id){
	while(head != NULL){
		if(head->id == id) return -1;
		head = head->next;
	}
	return 0;
}

static int insert_client_file_list(clients_file_queue **head, int id){
	if(check_client_id((*head), id) == -1) return -1;
	clients_file_queue *new = (clients_file_queue *) malloc(sizeof(clients_file_queue));
	new->id = id;
	new->next = (*head);
	(*head) = new;	
	return 0;
}

static int remove_client_file_list(clients_file_queue **head, int id){
	clients_file_queue *scanner = (* head);
	clients_file_queue *befree = NULL;
	if((* head)->id == id){
		befree = (* head);
		(* head) = (*head)->next;
		free(befree);
		return 0;
	}
	while(true){
		if(scanner->next == NULL) return -1;
		if(scanner->next->id == id){
			befree = scanner->next;
			scanner->next = scanner->next->next;
			free(befree);
			return 0;
		}
		scanner = scanner->next;
	}
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
		response->code[0] = FILE_OPERATION_FAILED;
		return -1;
	}
	if(!file_exists){
		init_file(client_id, filename, lock_file);
	}
	if(file_exists){
		start_write(file_index);
		if(server_storage.storage_table[file_index]->whos_locking != client_id){
			stop_write(file_index);
			response->code[0] = FILE_OPERATION_FAILED | FILE_LOCKED_BY_OTHERS;
			return -1;
		}
		if(insert_client_file_list(&server_storage.storage_table[file_index]->clients_open, client_id) < 0){
			stop_write(file_index);
			response->code[0] = FILE_OPERATION_FAILED | FILE_ALREADY_OPEN;
			return -1;
		}
 		if(lock_file){
			if(server_storage.storage_table[file_index]->whos_locking == -1)
				server_storage.storage_table[file_index]->whos_locking = client_id;
			else{
				remove_client_file_list(&server_storage.storage_table[file_index]->clients_open, client_id);
				stop_write(file_index);
				response->code[0] = FILE_OPERATION_FAILED | FILE_LOCKED_BY_OTHERS;
				response->code[1] = EBUSY;
				return -1;
			}
		}
		stop_write(file_index);
	}
	response->code[0] = FILE_OPERATION_SUCCESS;
	return 0;
}

int close_file(char *filename, int client_id, server_response *response){
	int file_index = search_file(filename);
	if(file_index == -1){
		response->code[0] = FILE_OPERATION_FAILED | FILE_NOT_EXISTS;
		response->code[1] = ENOENT;
		return -1; 
	}
	start_write(file_index);
	if(server_storage.storage_table[file_index]->whos_locking != client_id || server_storage.storage_table[file_index]->whos_locking != -1){
		stop_write(file_index);
		response->code[0] = FILE_OPERATION_FAILED | FILE_LOCKED_BY_OTHERS;
		response->code[1] = EACCES;
		return -1; 
	}
	
	if(remove_client_file_list(&server_storage.storage_table[file_index]->clients_open, client_id) < 0){
		stop_write(file_index);
		response->code[0] = FILE_OPERATION_FAILED;
		return -1; 
	}
	stop_write(file_index);
	response->code[0] = FILE_OPERATION_SUCCESS;
	return 0;
}

int remove_file(char *filename, int client_id,  server_response *response){
	int file_index = search_file(filename);
	if(file_index == -1){
		response->code[0] = FILE_OPERATION_FAILED | FILE_NOT_EXISTS;
		response->code[1] = ENOENT;
		return -1; 
	}

	start_write(file_index);
	if(server_storage.storage_table[file_index]->whos_locking != client_id){
		stop_write(file_index);
		response->code[0] = FILE_OPERATION_FAILED | FILE_LOCKED_BY_OTHERS;
		response->code[1] = EACCES;
		return -1; 
	}
	server_storage.storage_table[file_index]->deleted = true;
	free(server_storage.storage_table[file_index]->data);
	server_storage.storage_table[file_index]->size = 0;
	stop_write(file_index);
	SAFELOCK(storage_access_mtx);
	server_storage.size -= server_storage.storage_table[file_index]->size;
	server_storage.file_count -= 1;
	SAFEUNLOCK(storage_access_mtx);
	response->code[0] = FILE_OPERATION_SUCCESS;
	return 0;
}

int read_file(char *filename, unsigned char **buffer, int client_id, server_response *response){
	int file_index = search_file(filename);
	if(file_index == -1){
		response->code[0] = FILE_NOT_EXISTS | FILE_OPERATION_FAILED;
		response->code[1] = ENOENT;
		return -1;
	}
	/* QUI INIZIA IL LETTORE */
	start_read(file_index);
	/* QUI INIZIO A LEGGERE */
	if(check_client_id(server_storage.storage_table[file_index]->clients_open, client_id) == -1 && 
								(server_storage.storage_table[file_index]->whos_locking == -1 || server_storage.storage_table[file_index]->whos_locking == client_id)){
		*buffer = (unsigned char *) calloc(server_storage.storage_table[file_index]->size, sizeof(unsigned char));
		CHECKALLOC(*buffer, "Errore allocazione memoria read_file");
		response->size = server_storage.storage_table[file_index]->size;
		memcpy(*buffer, server_storage.storage_table[file_index]->data, response->size);
		if(server_storage.storage_table[file_index]->use_stat < 2)
			server_storage.storage_table[file_index]->use_stat += 1;


		/* QUI HO FINITO DI LEGGERE ED ESCO */
		stop_read(file_index);
		response->code[0] = FILE_OPERATION_SUCCESS;
		return 0;	
	}
	if(server_storage.storage_table[file_index]->whos_locking != -1 && server_storage.storage_table[file_index]->whos_locking != client_id){
		stop_read(file_index);
		response->code[1] = EACCES;
		response->code[0] = FILE_OPERATION_FAILED | FILE_LOCKED_BY_OTHERS;
		return -1;
	}
	stop_read(file_index);
	response->code[0] = FILE_OPERATION_FAILED;
	response->code[1] = EACCES;
	return -1;

}

int write_to_file(unsigned char *data, int length, char *filename, int client_id, server_response *response){
	int file_index = search_file(filename);
	int old_size = 0;
	if(file_index == -1){
		response->code[1] = ENOENT;
		response->code[0] = FILE_OPERATION_FAILED | FILE_NOT_EXISTS;
		return -1;
	}
	/* QUI INIZIA LO SCRITTORE */
	start_write(file_index);

	/* QUI SI SCRIVE */
	if(server_storage.storage_table[file_index]->whos_locking == client_id){ // If a file is locked, it's already open
		old_size = server_storage.storage_table[file_index]->size;
		if(check_memory(length) < 0){
			stop_write(file_index);
			response->code[1] = EFBIG;
			response->code[0] = FILE_OPERATION_FAILED;
			return -1;
		}
		

		server_storage.storage_table[file_index]->data = (unsigned char *) realloc(server_storage.storage_table[file_index]->data, length);
		CHECKALLOC(server_storage.storage_table[file_index], "Errore allocazione wite_to_file");
		memcpy(server_storage.storage_table[file_index]->data, data, length);
		server_storage.storage_table[file_index]->size = length;
		server_storage.storage_table[file_index]->last_modified = time(NULL);
		
		/* QUI HO FINITO DI SCRIVERE ED ESCO */
		stop_write(file_index);
		SAFELOCK(storage_access_mtx);
		server_storage.size += (length - old_size);
		SAFEUNLOCK(storage_access_mtx);
		response->code[0] = FILE_OPERATION_SUCCESS;
		
		return 0;
	}
	stop_write(file_index);
	response->code[1] = EACCES;
	response->code[0] = FILE_OPERATION_FAILED | FILE_NOT_LOCKED;
	return -1;
	
}

int append_to_file(unsigned char* new_data, int new_data_size, char *filename, int client_id, server_response *response){
	int file_index = search_file(filename);
	int old_size = 0;
	if(file_index == -1){
		response->code[0] = FILE_OPERATION_FAILED | FILE_NOT_EXISTS;
		return -1;
	}
	/* QUI INIZIA LO SCRITTORE */
	start_write(file_index);
	if(check_client_id(server_storage.storage_table[file_index]->clients_open, client_id) == -1 && 
								(server_storage.storage_table[file_index]->whos_locking == -1 || server_storage.storage_table[file_index]->whos_locking == client_id)){

		old_size = server_storage.storage_table[file_index]->size;
		if(check_memory(new_data_size + old_size) < 0){
			stop_write(file_index);
			response->code[1] = EFBIG;
			response->code[0] = FILE_OPERATION_FAILED;
			return -1;
		}
		
		server_storage.storage_table[file_index]->data = (unsigned char *) realloc(server_storage.storage_table[file_index]->data, new_data_size + old_size);
		CHECKALLOC(server_storage.storage_table[file_index], "Errore allocazione append_to_file");
		for (size_t i = old_size, j = 0; j < new_data_size; i++, j++)
			server_storage.storage_table[file_index]->data[i] = new_data[j];

		server_storage.storage_table[file_index]->size = new_data_size + old_size;
		server_storage.storage_table[file_index]->last_modified = time(NULL);

		stop_write(file_index);
		SAFELOCK(storage_access_mtx);
		server_storage.size += new_data_size;
		SAFEUNLOCK(storage_access_mtx);
		response->code[0] = FILE_OPERATION_SUCCESS;
		return 0;
	}
	if(server_storage.storage_table[file_index]->whos_locking != -1 && server_storage.storage_table[file_index]->whos_locking != client_id){
		stop_write(file_index);
		response->code[1] = EACCES;
		response->code[0] = FILE_OPERATION_FAILED | FILE_LOCKED_BY_OTHERS;
		return -1;
	}
	stop_write(file_index);
	response->code[1] = EACCES;
	response->code[0] = FILE_OPERATION_FAILED;
	return -1;

}

int lock_file(char *filename, int client_id, server_response *response){
	int file_index = search_file(filename);
	if(file_index == -1){
		response->code[0] = FILE_NOT_EXISTS | FILE_OPERATION_FAILED;
		response->code[1] = ENOENT;
		return -1;
	}
	/* QUI INIZIA LO SCRITTORE */
	start_write(file_index);
	if(server_storage.storage_table[file_index]->whos_locking == -1){
		server_storage.storage_table[file_index]->whos_locking = client_id;
		stop_write(file_index);
		response->code[0] = FILE_OPERATION_SUCCESS;
		return 0;
	}
	else if(server_storage.storage_table[file_index]->whos_locking == client_id){
		stop_write(file_index);
		response->code[0] = FILE_ALREADY_LOCKED | FILE_OPERATION_FAILED;
		response->code[1] = EINVAL;
		return -1;
	}
	stop_write(file_index);
	response->code[0] = FILE_LOCKED_BY_OTHERS | FILE_OPERATION_FAILED;
	response->code[1] = EBUSY;
	return -1;
}

int unlock_file(char *filename, int client_id, server_response *response){
	int file_index = search_file(filename);
	if(file_index == -1){
		response->code[0] = FILE_NOT_EXISTS | FILE_OPERATION_FAILED;
		response->code[1] = ENOENT;
		return -1;
	}
	/* QUI INIZIA LO SCRITTORE */
	start_write(file_index);
	if(server_storage.storage_table[file_index]->whos_locking == client_id){
		server_storage.storage_table[file_index]->whos_locking = -1;
		stop_write(file_index);
		response->code[0] = FILE_OPERATION_SUCCESS;
		return 0;
	}
	else if(server_storage.storage_table[file_index]->whos_locking != client_id){
		stop_write(file_index);
		response->code[0] = FILE_LOCKED_BY_OTHERS;
		response->code[1] = EBUSY;
		return -1;
	}
	stop_write(file_index);
	response->code[0] = FILE_NOT_LOCKED | FILE_OPERATION_FAILED;
	return -1;
}

void clean_storage(){
	for(int i = 0; i < 2*server_storage.file_limit; i++){
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
static unsigned int search_file(const char* pathname){
	int i = 0, max_len = 0, index = 0, path_len = 0;
	path_len = strlen(pathname);

	SAFELOCK(storage_access_mtx);
	max_len = 2*server_storage.file_limit;
	SAFEUNLOCK(storage_access_mtx);
	while(true){
		
		index = hash_val(pathname, i, max_len, path_len);
		SAFELOCK(storage_access_mtx);
		if(server_storage.storage_table[index] == NULL){
			SAFEUNLOCK(storage_access_mtx);
			return -1; // File not found
		}
		SAFEUNLOCK(storage_access_mtx);

		start_read(index);
		if(server_storage.storage_table[index]->deleted && strcmp(server_storage.storage_table[index]->name, pathname) == 0){
			stop_read(index);
			return -1; // File eliminato
		}
		else if(strcmp(server_storage.storage_table[index]->name, pathname) == 0){
			stop_read(index);
			return index; // Ho trovato il file
		}
		stop_read(index);
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

static unsigned int get_free_index(const char* pathname){
	int i = 0, max_len = 2*server_storage.file_limit, index = 0;
	unsigned int path_len = strlen(pathname);
	
	while(true){
		index =  hash_val(pathname, i, max_len, path_len);
		SAFELOCK(storage_access_mtx);
		if(server_storage.storage_table[index] == NULL){
			SAFEUNLOCK(storage_access_mtx);
			return index; // Il primo hash e' libero
		}
		SAFEUNLOCK(storage_access_mtx);

		start_read(index);

		if(server_storage.storage_table[index]->deleted){
			stop_read(index);
			return index; // Sovrascrivo una vecchia cella
		}
		stop_read(index);
		i++; // Non e' un posto libero, vado avanti
	}
}

static void init_file(int id, char *filename, bool locked){
	int index = get_free_index(filename);
	SAFELOCK(storage_access_mtx);
	server_storage.file_count += 1;
	if(server_storage.file_count > server_storage.max_file_num_reached)
		server_storage.max_file_num_reached += 1;
	server_storage.storage_table[index] = (fssFile *) malloc(sizeof(fssFile));
	CHECKALLOC(server_storage.storage_table[index], "Errore inserimento nuovo file");
	memset(server_storage.storage_table[index], 0, sizeof(fssFile));
	insert_client_file_list(&server_storage.storage_table[index]->clients_open, id);
	server_storage.storage_table[index]->create_time = time(NULL);
	server_storage.storage_table[index]->last_modified = time(NULL);
	memset(&server_storage.storage_table[index]->order_mutex, 0, sizeof(pthread_mutex_t));
	memset(&server_storage.storage_table[index]->access_mutex, 0, sizeof(pthread_mutex_t));
	memset(&server_storage.storage_table[index]->go_cond, 0, sizeof(pthread_cond_t));
	
	if(pthread_mutex_init(&server_storage.storage_table[index]->order_mutex, NULL) != 0){
		fprintf(stderr, "Errore di inizializzazione order mutex\n");
		exit(EXIT_FAILURE);
	}
	if(pthread_mutex_init(&server_storage.storage_table[index]->access_mutex, NULL) != 0){
		fprintf(stderr, "Errore di inizializzazione access mutex\n");
		exit(EXIT_FAILURE);
	}
	if(pthread_cond_init(&server_storage.storage_table[index]->go_cond, NULL) != 0){
		fprintf(stderr, "Errore di inizializzazione go condition\n");
		exit(EXIT_FAILURE);
	}

	server_storage.storage_table[index]->deleted = false;
	if(locked) server_storage.storage_table[index]->whos_locking = id;
	else server_storage.storage_table[index]->whos_locking = -1;
	server_storage.storage_table[index]->name = (char *)calloc(strlen(filename) + 1, sizeof(char));
	CHECKALLOC(server_storage.storage_table[index]->name, "Errore inserimento nuovo file");
	strncpy(server_storage.storage_table[index]->name, filename, strlen(filename));
	server_storage.storage_table[index]->use_stat = 2;
	server_storage.storage_table[index]->readers = 0;
	server_storage.storage_table[index]->writers = 0;
	SAFEUNLOCK(storage_access_mtx);
}

void* clock_algorithm(void *args){ // Lui va a diritto
	int table_size = 0;
	SAFELOCK(storage_access_mtx);
	table_size = 2*server_storage.file_limit;
	SAFEUNLOCK(storage_access_mtx);
	while(true){
		for(int i = 0; i < table_size; i++){
			SAFELOCK(storage_access_mtx);
			if(server_storage.storage_table[i] == NULL){
				SAFEUNLOCK(storage_access_mtx);
				continue;
			}
			SAFEUNLOCK(storage_access_mtx);

			start_write(i);
			if(server_storage.storage_table[i]->use_stat != 0 && !server_storage.storage_table[i]->deleted)
				server_storage.storage_table[i]->use_stat -= 1;
			stop_write(i);
			}
		}
		sleep(5); // Provvisorio
	}


void start_read(int file_index){
	SAFELOCK(server_storage.storage_table[file_index]->order_mutex); // ACQUIRE ORDER
	SAFELOCK(server_storage.storage_table[file_index]->access_mutex); // ACQUIRE ACCESS
	while (server_storage.storage_table[file_index]->writers > 0){
		if(pthread_cond_wait(&server_storage.storage_table[file_index]->go_cond, &server_storage.storage_table[file_index]->access_mutex) != 0){
			fprintf(stderr, "Errore (file %s, linea %d): wait su go_cond non riuscita\n", __FILE__, __LINE__);
			exit(EXIT_FAILURE);
		}
	}
	server_storage.storage_table[file_index]->readers += 1;
	SAFEUNLOCK(server_storage.storage_table[file_index]->order_mutex);
	SAFEUNLOCK(server_storage.storage_table[file_index]->access_mutex);
}

void stop_read(int file_index){
	SAFELOCK(server_storage.storage_table[file_index]->access_mutex); 
	server_storage.storage_table[file_index]->readers -= 1;
	if(server_storage.storage_table[file_index]->readers == 0){
		if(pthread_cond_signal(&server_storage.storage_table[file_index]->go_cond) < 0){
			fprintf(stderr, "Errore (file %s, linea %d): signal su go_cond non riuscita\n", __FILE__, __LINE__);
			exit(EXIT_FAILURE);
		}
	}
	SAFEUNLOCK(server_storage.storage_table[file_index]->access_mutex);
}

void start_write(int file_index){
	SAFELOCK(server_storage.storage_table[file_index]->order_mutex); // ACQUIRE ORDER
	SAFELOCK(server_storage.storage_table[file_index]->access_mutex); // ACQUIRE ACCESS
	while (server_storage.storage_table[file_index]->readers > 0 || server_storage.storage_table[file_index]->writers > 0){
		if(pthread_cond_wait(&server_storage.storage_table[file_index]->go_cond, &server_storage.storage_table[file_index]->access_mutex) != 0){
			fprintf(stderr, "Errore (file %s, linea %d): wait su go_cond non riuscita\n", __FILE__, __LINE__);
			exit(EXIT_FAILURE);
		}
	}
}

void stop_write(int file_index){
	SAFELOCK(server_storage.storage_table[file_index]->access_mutex); 
	server_storage.storage_table[file_index]->writers -= 1;
	if(pthread_cond_signal(&server_storage.storage_table[file_index]->go_cond) < 0){
		fprintf(stderr, "Errore (file %s, linea %d): signal su go_cond non riuscita\n", __FILE__, __LINE__);
		exit(EXIT_FAILURE);
	}
	SAFEUNLOCK(server_storage.storage_table[file_index]->access_mutex);
}



static inline unsigned int hash_pjw(const void* key){
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

static inline unsigned int fnv_hash_function(const void *key, int len) {
    unsigned char *p = (unsigned char*) key;
    unsigned int h = 2166136261u;
    int i;
    for ( i = 0; i < len; i++ )
        h = ( h * 16777619 ) ^ p[i];
    return h;
}

static unsigned int hash_val(const void* key, unsigned int i, unsigned int max_len, unsigned int key_len){
	return ((hash_pjw(key) + i*fnv_hash_function(key, key_len)) % max_len);
}