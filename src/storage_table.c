#include "storage_table.h"


extern int respond_to_client(int com, server_response response);
extern int sendback_client(int com, bool done);
extern void lock_next(char* pathname, bool mutex_write);

void init_table(int max_file_num, int max_size){
	server_storage.storage_table = (fssFile **) malloc(max_file_num*sizeof(fssFile *));
	memset(server_storage.storage_table, 0, max_file_num*sizeof(fssFile *));
	server_storage.file_limit = max_file_num; // nbuckets
	server_storage.size_limit = max_size;
	server_storage.size = 0;
	server_storage.max_size_reached = 0;
	server_storage.max_file_num_reached = 0;
	server_storage.file_count = 0; // nentries
	server_storage.total_evictions = 0;
	pthread_mutex_init(&server_storage.storage_access_mtx, NULL);
}

void clean_attributes(fssFile **entry, bool close_com){
	open_file_client_list *befree = NULL;
	lock_file_queue *befree1 = NULL;
	server_response response;
	if((*entry)->clients_open != NULL){
		while ((*entry)->clients_open != NULL){
			befree = (*entry)->clients_open;
			(*entry)->clients_open = (*entry)->clients_open->next;
			free(befree);
		}
	}
	if((*entry)->lock_waiters != NULL){
		while ((*entry)->lock_waiters != NULL){
			if(close_com) close((*entry)->lock_waiters->com);
			else{
				memset(&response, 0, sizeof response);
				response.code[0] = FILE_OPERATION_FAILED | FILE_NOT_EXISTS;
				response.code[1] = ENOENT;
				response.pathlen = strlen((*entry)->name) + 1;
				response.pathname = (char *) calloc(response.pathlen, sizeof(char));
				strcpy(response.pathname, (*entry)->name);
				if((*entry)->lock_waiters != NULL){
					while ((*entry)->lock_waiters != NULL){
						respond_to_client((*entry)->lock_waiters->com, response);
						sendback_client((*entry)->lock_waiters->com, false);
						befree = (*entry)->lock_waiters;
						(*entry)->lock_waiters = (*entry)->lock_waiters->next;
						free(befree);
					}
				}
				free(response.pathname);
			}
			befree1 = (*entry)->lock_waiters;
			(*entry)->lock_waiters = (*entry)->lock_waiters->next;
			free(befree1);
		}
	}
}

static int compare(const void *a, const void *b) {
	victim a1 = *(victim *)a, b1 = *(victim *)b; 
	time_t now = time(NULL);
	if((a1.use_stat - b1.use_stat) != 0)
		return a1.use_stat - b1.use_stat; // sort by use stat

	else return (now - a1.last_modified) - (now - b1.last_modified); // if use stat is the same, sort by age
}

static int select_victim(char* caller, int files_to_delete, unsigned long memory_to_free) {
	victim* victims = NULL;
	fssFile* entry = NULL;
	int counter = 0, j = 0;
	unsigned long memory_freed = 0;
	SAFELOCK(server_storage.storage_access_mtx);
	victims = (victim *) calloc(server_storage.file_count, sizeof(victim));
	server_storage.total_evictions += 1;
	printf("SIZE: %d\n", server_storage.file_limit);
	for (size_t i = 0; i < server_storage.file_limit; i++){
		entry = server_storage.storage_table[i];
		while(entry){
			if(caller && strncmp(entry->name, caller, sizeof(entry->name)) == 0) continue;
			start_read(entry);
			victims[counter].pathname = (char*) calloc(strlen(entry->name)+1, sizeof(char));
			CHECKALLOC(victims[counter].pathname, "Errore allocazione pathname select_victim");
			strncpy(victims[counter].pathname, entry->name, strlen(entry->name));
			victims[counter].create_time = entry->create_time;
			victims[counter].last_modified = entry->last_modified;
			victims[counter].use_stat = entry->use_stat;
			victims[counter].size = entry->size;
			stop_read(entry);
			counter++;
		}
	}
	SAFEUNLOCK(server_storage.storage_access_mtx);
	
	
	qsort(victims, counter, sizeof(victim), compare);
	if(files_to_delete){
		delete_entry(-1, victims[0].pathname);
		for (int i = 0; i < counter; i++)
			free(victims->pathname);
		
		free(victims);
		return 0;
	}
	while(j < counter && memory_freed < memory_to_free){
		delete_entry(-1, victims[0].pathname);
		memory_freed = victims[j].size;
		j++;
	}
	for (int i = 0; i < counter; i++)
		free(victims->pathname);
		
	free(victims);
	return 0;
}
/**
 * Check whether there's enough space in memory or not
 * 
 * @param new_size size of the file to be inserted
 * 
 * @returns 0 if the operation is successful, -1 if the file is too big or there are no locked files to remove
 *
 */
int check_memory(unsigned long new_size, unsigned long old_size, char* caller){
	unsigned long max_capacity = server_storage.size_limit, size_used = 0;
	if(new_size + old_size > server_storage.size_limit)
		return -1;

	SAFELOCK(server_storage.storage_access_mtx);
	size_used = server_storage.size;
	SAFEUNLOCK(server_storage.storage_access_mtx);
	if(size_used + new_size <= server_storage.size_limit) return 0;
	return select_victim(caller, 0, (new_size + size_used) - server_storage.size_limit);
}



int check_count(){
	int file_count = 0, file_limit = 0;
	file_limit = server_storage.file_limit;
	SAFELOCK(server_storage.storage_access_mtx);
	file_count = server_storage.file_count;
	if(file_count + 1 <= file_limit){
		SAFEUNLOCK(server_storage.storage_access_mtx);
		return 0;
	}
	SAFEUNLOCK(server_storage.storage_access_mtx);
	return select_victim(NULL, 1, 0);
}

static int check_client_id(open_file_client_list *head, int id){
	while(head != NULL){
		if(head->id == id) return -1;
		head = head->next;
	}
	return 0;
}

/**
 * Insert client id in the open file list
 * 
 * @param head double pointer to the head of the list
 * @param id the id of the client to add
 * 
 * @returns 0 if successful, -1 the client is already in the list 
 *
 */
static int insert_client_file_list(open_file_client_list **head, int id){
	if(check_client_id((*head), id) == -1) return -1;
	open_file_client_list *new = (open_file_client_list *) malloc(sizeof(open_file_client_list));
	new->id = id;
	new->next = (*head);
	(*head) = new;	
	return 0;
}

int create_new_entry(int id, char *filename, bool locked){
	int index = hash_pjw(filename);
	fssFile *new_entry = NULL;
	SAFELOCK(server_storage.storage_access_mtx);
	new_entry = server_storage.storage_table[index];
	
	// for (new_entry = server_storage.storage_table[index]; new_entry; new_entry = new_entry->next)
	// 	if(strncmp(filename, new_entry->name, strlen(filename) == 0)){
	// 		SAFEUNLOCK(server_storage.storage_access_mtx);
	// 		return -1; // Aready in
	// 	}
			
	check_count();
	// Update server_storage info
	server_storage.file_count += 1;
	if(server_storage.file_count > server_storage.max_file_num_reached)
		server_storage.max_file_num_reached += 1;

	// init new file
	new_entry = (fssFile *)calloc(1, sizeof(fssFile));
	CHECKALLOC(new_entry, "Errore inserimento nuovo file");
	insert_client_file_list(&new_entry->clients_open, id);
	new_entry->create_time = time(NULL);
	new_entry->last_modified = time(NULL);

	if(pthread_mutex_init(&new_entry->order_mutex, NULL) != 0){
		fprintf(stderr, "Errore di inizializzazione order mutex\n");
		exit(EXIT_FAILURE);
	}
	if(pthread_mutex_init(&new_entry->access_mutex, NULL) != 0){
		fprintf(stderr, "Errore di inizializzazione access mutex\n");
		exit(EXIT_FAILURE);
	}
	if(pthread_cond_init(&new_entry->go_cond, NULL) != 0){
		fprintf(stderr, "Errore di inizializzazione go condition\n");
		exit(EXIT_FAILURE);
	}
	
	if(locked) new_entry->whos_locking = id;
	else new_entry->whos_locking = -1;
	new_entry->name = (char *)calloc(strlen(filename) + 1, sizeof(char));
	CHECKALLOC(new_entry->name, "Errore inserimento nuovo file");
	strncpy(new_entry->name, filename, strlen(filename));
	new_entry->use_stat = 16;
	new_entry->readers = 0;
	new_entry->writers = 0;
	new_entry->next = server_storage.storage_table[index];
	server_storage.storage_table[index] = new_entry;
	SAFEUNLOCK(server_storage.storage_access_mtx);
	return 0;
}

int write_entry(int id, char *pathname, unsigned char *data, size_t data_len, unsigned char op){
	fssFile* entry = search_file(pathname);
	if(!entry)
		return -1; // ENOENT
	
	if(check_memory(data_len, pathname) < 2)
		return -2; // EFBIG
	
	start_write(&entry);
	if(op == WRITE){
		if(entry->whos_locking != id || check_client_id(entry->clients_open, id) < 0 || entry->data){ // check if clients open is needed
			stop_write(&entry);
			return -3; // EACCESS
		}
		// check storage size and evict
		entry->data = (unsigned char *) calloc(data_len, sizeof(unsigned char));
		CHECKALLOC(entry->data, "Errore allocazione WRITE");
		memcpy(entry->data, data, data_len);
		entry->last_modified = time(NULL);
		entry->size = data_len;
		entry->use_stat += 1;
	}
	else if(op == APPEND){
		if(check_client_id(entry->clients_open, id) < 0){ // check if clients open is needed
			stop_write(&entry);
			return -4; // EBUSY
		}
		// check storage size and evict
		entry->data = (unsigned char *) realloc(entry->data, entry->data + data_len);
		CHECKALLOC(entry->data, "Errore allocazione APPEND");
		memcpy(entry->data + entry->size, data, data_len);
		entry->last_modified = time(NULL);
		entry->size += data_len;
		entry->use_stat += 1;
	}
	SAFELOCK(server_storage.storage_access_mtx);
	server_storage.size += data_len;
	if(server_storage.size > server_storage.max_size_reached)
		server_storage.max_size_reached = server_storage.size;
	SAFEUNLOCK(server_storage.storage_access_mtx);

	stop_write(&entry);
	return 0;
}

int save_entry_to_buffer(int id, char *pathname, unsigned char* buffer, size_t *buffer_size){
	fssFile* entry = search_file(pathname);
	if(!entry)
		return -1; // ENOENT
	
	start_read(&entry);
	if(check_client_id(entry->clients_open, id) < 0){
		stop_read(&entry);
		return -2; // EACCESS
	}
		
	if(entry->whos_locking != ){
		start_read(&entry);
		return -3; // 
	}

}

int delete_entry(int id, char *pathname){
	int index = hash_pjw(pathname);
	fssFile* entry = NULL;
	fssFile* prev = NULL;
	SAFELOCK(server_storage.storage_access_mtx);
	for (entry = server_storage.storage_table[index]; entry; prev = entry, entry = entry->next){
		start_read(&entry);
		if(strncmp(pathname, entry->name, strlen(pathname) == 0)){
			stop_read(&entry);
			SAFEUNLOCK(server_storage.storage_access_mtx);
			break;
		}
		stop_read(&entry);
	}
	SAFEUNLOCK(server_storage.storage_access_mtx);
	if(!entry){
		return -1; // ENOENT
	}
	start_write(&entry);
	entry->data = (char *) realloc(entry->data, 11);
	strncpy(entry->data, "deleted", 10);
	stop_write(&entry);
	if(!prev){
		SAFELOCK(server_storage.storage_access_mtx);
		server_storage.storage_table[index] = entry->next;
		SAFEUNLOCK(server_storage.storage_access_mtx);
	}
	else prev->next = entry->next;
	SAFELOCK(server_storage.storage_access_mtx);
	server_storage.file_count -= 1;
	server_storage.size -= entry->size;
	SAFEUNLOCK(server_storage.storage_access_mtx);
	free(entry->name);
	if(entry->data) free(entry->data);
	CHECKSCEXIT(pthread_mutex_destroy(&entry->access_mutex), false, "Errore pthread_mutex_destroy");
	CHECKSCEXIT(pthread_mutex_destroy(&entry->order_mutex), false, "Errore pthread_mutex_destroy");
	CHECKSCEXIT(pthread_cond_destroy(&entry->go_cond), false, "Errore pthread_cond_destroy");
	clean_attributes(&entry, false);
	free(entry);
	return 0;
}




/**
 * Search if an entry is already in the hash table
 * 
 * @param pathname the file to be searched
 * 
 * @returns entry searched or NULL 
 *
 */
fssFile* search_file(const char* pathname){
	int index = hash_pjw(pathname);
	fssFile* entry = NULL;
	SAFELOCK(server_storage.storage_access_mtx);
	for (entry = server_storage.storage_table[index]; entry; entry = entry->next){
		start_read(&entry);
		if(strncmp(pathname, entry->name, strlen(pathname) == 0)){
			stop_read(&entry);
			SAFEUNLOCK(server_storage.storage_access_mtx);
			return entry; // Aready in
		}
		stop_read(&entry);
	}

	SAFEUNLOCK(server_storage.storage_access_mtx);
	return NULL;
}

static unsigned int hash_pjw(void* key){
    char *datum = (char *)key;
    unsigned int hash_value, i;

    if(!datum) return 0;

    for (hash_value = 0; *datum; ++datum) {
        hash_value = (hash_value << ONE_EIGHTH) + *datum;
        if ((i = hash_value & HIGH_BITS) != 0)
            hash_value = (hash_value ^ (i >> THREE_QUARTERS)) & ~HIGH_BITS;
    }
    return (hash_value) % server_storage.file_limit;
}

void start_read(const fssFile** entry){
	fssFile* file = (*entry);
	SAFELOCK(file->order_mutex); // ACQUIRE ORDER
	SAFELOCK(file->access_mutex); // ACQUIRE ACCESS
	while (file->writers > 0){
		if(pthread_cond_wait(&file->go_cond, &file->access_mutex) != 0){
			fprintf(stderr, "Errore (file %s, linea %d): wait su go_cond non riuscita\n", __FILE__, __LINE__);
			exit(EXIT_FAILURE);
		}
	}
	file->readers += 1;
	SAFEUNLOCK(file->order_mutex);
	SAFEUNLOCK(file->access_mutex);
}

void stop_read(const fssFile** entry){
	fssFile* file = (*entry);
	SAFELOCK(file->access_mutex); 
	file->readers -= 1;
	if(file->readers == 0){
		if(pthread_cond_signal(&file->go_cond) < 0){
			fprintf(stderr, "Errore (file %s, linea %d): signal su go_cond non riuscita\n", __FILE__, __LINE__);
			exit(EXIT_FAILURE);
		}
	}
	SAFEUNLOCK(file->access_mutex);

}

void start_write(const fssFile** entry){
	fssFile* file = (*entry);
	SAFELOCK(file->order_mutex); // ACQUIRE ORDER
	SAFELOCK(file->access_mutex); // ACQUIRE ACCESS
	while (file->readers > 0 || file->writers > 0){
		if(pthread_cond_wait(&file->go_cond, &file->access_mutex) != 0){
			fprintf(stderr, "Errore (file %s, linea %d): wait su go_cond non riuscita\n", __FILE__, __LINE__);
			exit(EXIT_FAILURE);
		}
	}
	file->writers += 1;
	SAFEUNLOCK(file->order_mutex); 
	SAFEUNLOCK(file->access_mutex); 
}

void stop_write(const fssFile** entry){
	fssFile* file = (*entry);
	SAFELOCK(file->access_mutex); 
	file->writers -= 1;
	if(pthread_cond_signal(&file->go_cond) < 0){
		fprintf(stderr, "Errore (file %s, linea %d): signal su go_cond non riuscita\n", __FILE__, __LINE__);
		exit(EXIT_FAILURE);
	}
	SAFEUNLOCK(file->access_mutex);
}