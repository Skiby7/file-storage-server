#include "file.h"


storage server_storage;

pthread_cond_t start_LFU_selector = PTHREAD_COND_INITIALIZER;
static void init_file(int id, char *filename, bool locked);
static fssFile* search_file(const char* pathname);
static unsigned int get_free_index(const char* pathname);
static unsigned int hash_val(const void* key, unsigned int i, unsigned int max_len, unsigned int key_len);
void start_read(fssFile* entry);
void start_write(fssFile* entry);
void stop_read(fssFile* entry);
void stop_write(fssFile* entry);
extern int respond_to_client(int com, server_response response);
extern int sendback_client(int com, bool done);
extern void lock_next(char* pathname, bool mutex_write);




/**
 * Check whether an id is already in the lock_file_queue or not
 * 
 * @param head pointer to the head of the queue
 * @param id th id of the client to check
 * 
 * @returns 0 if the id is not in the queue, else returns -1 
 *
 */
static int check_client_id_lock(lock_file_queue *head, int id){
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

/**
 * Insert client id in the lock_file_queue of the filename
 * 
 * @param filename pathname of the file
 * @param id the id of the client to add
 * @param com the file descriptor of the client
 * 
 * @returns 0 if successful, -1 the client is already in the list 
 *
 */
int insert_lock_file_list(char *filename, int id, int com){
	int file_index = search_file(filename);
	if(file_index < 0){
		return -1;
	}
	start_write(file_index);

	if(check_client_id_lock(server_storage.storage_table[file_index]->lock_waiters, id) == -1){
		stop_write(file_index);
		return -1;
	}
	lock_file_queue *new = (lock_file_queue *) malloc(sizeof(lock_file_queue));
	new->id = id;
	new->com = com;
	new->next = server_storage.storage_table[file_index]->lock_waiters;
	server_storage.storage_table[file_index]->lock_waiters = new;	
	stop_write(file_index);
	return 0;
}

/**
 * Pop client id in the lock_file_queue of the filename
 * 
 * @param filename pathname of the file
 * @param id pointer to return the id of the client popped
 * @param com pointer to return the com of the client popped
 * 
 * @returns 0 if successful, -1 the client is not in the queue 
 *
 */
int pop_lock_file_list(char *filename, int *id, int *com){
	int file_index = search_file(filename);
	start_write(file_index);

	lock_file_queue *scanner = server_storage.storage_table[file_index]->lock_waiters;
	if(scanner == NULL){
		stop_write(file_index);
		return -1;
	}
	if(scanner->next == NULL){
		*id = scanner->id;
		*com = scanner->com;
		free(scanner);	
		server_storage.storage_table[file_index]->lock_waiters = NULL;
		stop_write(file_index);
		return 0;
	}
	while(scanner->next->next != NULL) scanner = scanner->next;
	*id = scanner->next->id;
	*com = scanner->next->com;
	free(scanner->next);
	scanner->next = NULL;
	stop_write(file_index);
	return 0;
}

static int remove_client_file_list(open_file_client_list **head, int id){
	open_file_client_list *scanner = (* head);
	open_file_client_list *befree = NULL;
	if((*head) == NULL) return -1;
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


static int create_new_entry(int id, char *filename, bool locked){
	int index = hash_pjw(filename);
	fssFile *new_entry = NULL;
	SAFELOCK(server_storage.storage_access_mtx);
	new_entry = server_storage.storage_table[index];
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
/**
 * Open the file identified by filename with the specified flags: if the file not exists, O_CREATE must be passed, 
 * else if file exists and O_CREATE is passed, the operation fails
 * 
 * @param filename pathname of the file
 * @param flags O_CREATE to create the file, O_LOCK to lock the file
 * @param client_id id of the client opening the file
 * @param response pointer to the server_response
 * 
 * @returns set the response and returns 0 if successful, -1 if something went wrong
 *
 */
int open_file(char *filename, int flags, int client_id, server_response *response){
	fssFile* file = search_file(filename);
	bool create_file = (flags & O_CREATE);
	bool lock_file = (flags & O_LOCK);
	bool file_exists = file ? true : false;
	
	if(file_exists && create_file){
		response->code[0] = FILE_OPERATION_FAILED | FILE_EXISTS;
		return -1;
	}
	
	if(!file_exists && !create_file){
		response->code[0] = FILE_OPERATION_FAILED;
		return -1;
	}

	if(!file_exists){
		check_count();
		init_file(client_id, filename, lock_file);
	} 
	
	else if(file_exists){
		start_write(file);
		if(file->whos_locking != client_id && file->whos_locking != -1){
			stop_write(file);
			response->code[0] = FILE_OPERATION_FAILED | FILE_LOCKED_BY_OTHERS;
			return -1;
		}
		if(insert_client_file_list(&file->clients_open, client_id) < 0){
			stop_write(file);
			response->code[0] = FILE_ALREADY_OPEN;
			return 0;
		}
 		if(lock_file){
			if(file->whos_locking == -1) file->whos_locking = client_id;
			else{
				remove_client_file_list(&file->clients_open, client_id);
				stop_write(file);
				response->code[0] = FILE_OPERATION_FAILED | FILE_LOCKED_BY_OTHERS;
				response->code[1] = EBUSY;
				return -1;
			}
		}
		file->use_stat += 1;
		stop_write(file);
	}
	// SAFELOCK(storage_access_mtx);
	// pthread_cond_signal(&start_LFU_selector);
	// SAFEUNLOCK(storage_access_mtx);
	response->code[0] = FILE_OPERATION_SUCCESS;
	return 0;
}

/**
 * Close the file identified by filename
 * 
 * @param filename pathname of the file
 * @param client_id id of the client closing the file
 * @param response pointer to the server_response
 * 
 * @returns set the response and returns 0 if successful, -1 if something went wrong
 *
 */
int close_file(char *filename, int client_id, server_response *response){
	fssFile* file = search_file(filename);
	if(!file){
		response->code[0] = FILE_OPERATION_FAILED | FILE_NOT_EXISTS;
		response->code[1] = ENOENT;
		return -1; 
	}
	start_write(file);
	if(file->whos_locking != client_id && file->whos_locking != -1){
		stop_write(file);
		response->code[0] = FILE_OPERATION_FAILED | FILE_LOCKED_BY_OTHERS;
		response->code[1] = EACCES;
		return -1; 
	}
	
	if(remove_client_file_list(&file->clients_open, client_id) < 0){
		stop_write(file);
		response->code[0] = FILE_OPERATION_FAILED;
		return -1; 
	}
	stop_write(file);
	response->code[0] = FILE_OPERATION_SUCCESS;
	return 0;
}

int delete_entry(int id, char *pathname){
	int index = hash_pjw(pathname);
	fssFile* entry = NULL;
	fssFile* prev = NULL;
	SAFELOCK(server_storage.storage_access_mtx);
	for (entry = server_storage.storage_table[index]; entry; prev = entry, entry = entry->next){
		start_write(entry);
		if(strncmp(pathname, entry->name, strlen(pathname) == 0)){
			if(entry->whos_locking == id){
				entry->name = (char *) realloc(entry->name, 11);
				strncpy(entry->name, "deleted", 10);
				stop_write(entry);
				SAFEUNLOCK(server_storage.storage_access_mtx);
				break;
			}
			stop_write(entry);
			SAFEUNLOCK(server_storage.storage_access_mtx);
			return -1; // EACCESS
		}
		stop_write(entry);
	}
	SAFEUNLOCK(server_storage.storage_access_mtx);
	if(!entry){
		return -2; // ENOENT
	}
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
 * Remove the file identified by filename
 * 
 * @param filename pathname of the file
 * @param client_id id of the client removing the file
 * @param response pointer to the server_response
 * 
 * @returns set the response and returns 0 if successful, -1 if something went wrong
 *
 */
int remove_file(char *filename, int client_id,  server_response *response){
	fssFile* file = search_file(filename);
	int exit_status = delete_entry(client_id, filename);
	if(exit_status == -1){
		stop_write(file);
		response->code[0] = FILE_OPERATION_FAILED | FILE_LOCKED_BY_OTHERS;
		response->code[1] = EACCES;
		return -1; 
	}
	if(exit_status == -2){
		response->code[0] = FILE_OPERATION_FAILED | FILE_NOT_EXISTS;
		response->code[1] = ENOENT;
		return -1; 
	}
	response->code[0] = FILE_OPERATION_SUCCESS;
	return 0;
}

/**
 * Read the file identified by filename and copy the data in response->data
 * 
 * @param filename pathname of the file
 * @param client_id id of the client reading the file
 * @param response pointer to the server_response
 * 
 * @returns set the response and returns 0 if successful, -1 if something went wrong
 *
 */
int read_file(char *filename, int client_id, server_response *response){
	fssFile* file = search_file(filename);
	if(!file){
		response->code[0] = FILE_NOT_EXISTS | FILE_OPERATION_FAILED;
		response->code[1] = ENOENT;
		return -1;
	}
	/* QUI INIZIA IL LETTORE */
	start_read(file);
	/* QUI INIZIO A LEGGERE */
	if(check_client_id(file->clients_open, client_id) == -1 && (file->whos_locking == -1 || file->whos_locking == client_id)){
		response->data = (unsigned char *) calloc(file->size, sizeof(unsigned char));
		CHECKALLOC(response->data, "Errore allocazione memoria read_file");
		response->size = file->size;
		memcpy(response->data, file->data, response->size);
		file->use_stat += 1;

		/* QUI HO FINITO DI LEGGERE ED ESCO */
		stop_read(file);
		response->code[0] = FILE_OPERATION_SUCCESS;
		return 0;	
	}
	if(file->whos_locking != -1 && file->whos_locking != client_id){
		stop_read(file);
		response->code[1] = EACCES;
		response->code[0] = FILE_OPERATION_FAILED | FILE_LOCKED_BY_OTHERS;
		return -1;
	}
	stop_read(file);
	response->code[0] = FILE_OPERATION_FAILED;
	response->code[1] = EACCES;
	return -1;

}

/**
 * Read n files from the server
 * 
 * @param last_index last index of the storage table visited
 * @param client_id id of the client reading the file
 * @param response pointer to the server_response
 * 
 * @returns set the response and returns 0 if successful, -1 if something went wrong
 *
 */
int read_n_file(char **last_file, int client_id, server_response* response){
	int index = hash_pjw(*last_file);
	fssFile* entry = search_file(*last_file)->next;
	SAFELOCK(server_storage.storage_access_mtx);
	if(!entry){
		index++;
		while(index < server_storage.file_limit && server_storage.storage_table[index] == NULL) index++;
		entry = server_storage.storage_table[index];
	}

	while(true){
		start_read(entry);
		if(entry->whos_locking == -1 || entry->whos_locking == client_id){
			response->data = (unsigned char *) calloc(entry->size, sizeof(unsigned char));
			CHECKALLOC(response->data, "Errore allocazione memoria read_file");
			response->size = entry->size;
			memcpy(response->data, entry->data, response->size);
			response->pathlen = strlen(basename(entry->name))+1;
			response->pathname = (char *) calloc(response->pathlen, sizeof(char));
			strncpy(response->pathname, basename(entry->name), response->pathlen-1);
			/* QUI HO FINITO DI LEGGERE ED ESCO */
			stop_read(entry);
			SAFEUNLOCK(server_storage.storage_access_mtx);
			*last_file = (char *) realloc(*last_file, strlen(entry->name)+1);
			strcpy(*last_file, entry->name);
			response->code[0] = FILE_OPERATION_SUCCESS;
			return 0;	
		}
		stop_read(entry);
		if(entry->next){
			entry = entry->next;
			continue;
		} 
		index++;
		while(index < server_storage.file_limit && server_storage.storage_table[index] == NULL) index++;
		if(index == server_storage.file_limit) break;
		entry = server_storage.storage_table[index];
	}
	SAFEUNLOCK(server_storage.storage_access_mtx);
	response->code[0] = FILE_OPERATION_SUCCESS;
	response->size = 1;
	response->data = calloc(1, sizeof(unsigned char));
	response->data[0] = 0;
	return 1;
}

/**
 * Write data in the file identified by filename. The client identified by client_id must have performed 
 * an open_file with O_CREATE and O_LOCK set, otherwise the operation fails.
 * 
 * @param data data to be written
 * @param length length of data in bytes
 * @param pathname pathname of the file
 * @param client_id id of the client reading the file
 * @param response pointer to the server_response
 * 
 * @returns set the response and returns 0 if successful, -1 if something went wrong
 *
 */
int write_to_file(unsigned char *data, int length, char *pathname, int client_id, server_response *response){
	fssFile* file = search_file(pathname);
	// printf("FILE INDEX: %d\n", file_index);
	if(!file){
		response->code[1] = ENOENT;
		response->code[0] = FILE_OPERATION_FAILED | FILE_NOT_EXISTS;
		return -1;
	}
	if(check_memory(length, pathname) < 0){
		response->code[1] = EFBIG;
		response->code[0] = FILE_OPERATION_FAILED;
		return -1;
	}
	/* QUI INIZIA LO SCRITTORE */
	start_write(file);

	/* QUI SI SCRIVE */
	if(file->whos_locking == client_id){ // If a file is locked, it's already open
		
		file->data = (unsigned char *) realloc(file->data, length);
		CHECKALLOC(file, "Errore allocazione write_to_file");
		memcpy(file->data, data, length);
		file->size = length;
		file->use_stat += 1;
		file->last_modified = time(NULL);
		
		/* QUI HO FINITO DI SCRIVERE ED ESCO */
		stop_write(file);
		SAFELOCK(server_storage.storage_access_mtx);
		server_storage.size += length;
		if(server_storage.size > server_storage.max_size_reached)  server_storage.max_size_reached = server_storage.size;
		pthread_cond_signal(&start_LFU_selector);
		SAFEUNLOCK(server_storage.storage_access_mtx);
		response->code[0] = FILE_OPERATION_SUCCESS;
		
		return 0;
	}
	stop_write(file);
	response->code[1] = EACCES;
	response->code[0] = FILE_OPERATION_FAILED | FILE_NOT_LOCKED;
	return -1;
}

/**
 * Append new_data in the file identified by filename.
 * 
 * @param new_data data to be written
 * @param new_data_size length of new_data in bytes
 * @param pathname pathname of the file
 * @param client_id id of the client reading the file
 * @param response pointer to the server_response
 * 
 * @returns set the response and returns 0 if successful, -1 if something went wrong
 *
 */
int append_to_file(unsigned char* new_data, int new_data_size, char *pathname, int client_id, server_response *response){
	fssFile* file = search_file(pathname);
	int old_size = 0;
	if(!file){
		response->code[0] = FILE_OPERATION_FAILED | FILE_NOT_EXISTS;
		response->code[1] = ENOENT;
		return -1;
	}
	start_read(file);
	old_size = file->size;
	stop_read(file);
	if(check_memory(new_data_size, old_size, pathname) < 0){
		response->code[0] = FILE_OPERATION_FAILED;
		response->code[1] = EFBIG;
		return -1;
	}
	/* QUI INIZIA LO SCRITTORE */
	start_write(pathname);
	if(check_client_id(server_storage.storage_table[file_index]->clients_open, client_id) == -1 && 
								(server_storage.storage_table[file_index]->whos_locking == -1 || server_storage.storage_table[file_index]->whos_locking == client_id)){

		server_storage.storage_table[file_index]->data = (unsigned char *) realloc(server_storage.storage_table[file_index]->data, new_data_size + old_size);
		CHECKALLOC(server_storage.storage_table[file_index], "Errore allocazione append_to_file");
		memcpy(server_storage.storage_table[file_index]->data + old_size, new_data, new_data_size);
		// for (size_t i = old_size, j = 0; j < new_data_size; i++, j++)
		// 	server_storage.storage_table[file_index]->data[i] = new_data[j];

		server_storage.storage_table[file_index]->size = new_data_size + old_size;
		server_storage.storage_table[file_index]->use_stat += 1;
		server_storage.storage_table[file_index]->last_modified= time(NULL);

		stop_write(file_index);
		SAFELOCK(storage_access_mtx);
		server_storage.size += new_data_size;
		if(server_storage.size > server_storage.max_size_reached)  server_storage.max_size_reached = server_storage.size;
		pthread_cond_signal(&start_LFU_selector);
		SAFEUNLOCK(storage_access_mtx);
		response->code[0] = FILE_OPERATION_SUCCESS;
		return 0;
	}
	if(server_storage.storage_table[file_index]->whos_locking == -1 || server_storage.storage_table[file_index]->whos_locking == client_id){
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

/**
 * Lock the file identified by filename
 * 
 * @param filename pathname of the file
 * @param client_id id of the client reading the file
 * @param response pointer to the server_response
 * 
 * @returns set the response and returns 0 if successful, -1 if something went wrong
 *
 */
int lock_file(char *filename, int client_id, bool mutex_write, server_response *response){
	int file_index = search_file(filename);
	if(file_index == -1){
		response->code[0] = FILE_NOT_EXISTS | FILE_OPERATION_FAILED;
		response->code[1] = ENOENT;
		return -1;
	}
	/* QUI INIZIA LO SCRITTORE */
	if(mutex_write) start_write(file_index);
	if(server_storage.storage_table[file_index]->whos_locking == -1){
		server_storage.storage_table[file_index]->whos_locking = client_id;
		server_storage.storage_table[file_index]->use_stat += 1;
		if(mutex_write) stop_write(file_index);
		response->code[0] = FILE_OPERATION_SUCCESS;
		return 0;
	}
	else if(server_storage.storage_table[file_index]->whos_locking == client_id){
		if(mutex_write) stop_write(file_index);
		response->code[0] = FILE_ALREADY_LOCKED | FILE_OPERATION_FAILED;
		response->code[1] = EINVAL;
		return -1;
	}
	if(mutex_write) stop_write(file_index);
	response->code[0] = FILE_LOCKED_BY_OTHERS | FILE_OPERATION_FAILED;
	response->code[1] = EBUSY;
	return -1;
}

/**
 * Unlock the file identified by filename
 * 
 * @param filename pathname of the file
 * @param client_id id of the client reading the file
 * @param response pointer to the server_response
 * 
 * @returns set the response and returns 0 if successful, -1 if something went wrong
 *
 */
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

void print_summary(){
	char memory[20];
	char files[20];
	snprintf(memory, 20, "%lu/%lu", server_storage.max_size_reached, server_storage.size_limit); 
	snprintf(files, 20, "%u/%u", server_storage.max_file_num_reached, server_storage.file_limit); 
	printf(ANSI_COLOR_GREEN CONF_LINE_TOP
		"│ %-12s\t"ANSI_COLOR_YELLOW"%20s"ANSI_COLOR_GREEN" │\n" CONF_LINE
		"│ %-12s\t"ANSI_COLOR_YELLOW"%20s"ANSI_COLOR_GREEN" │\n" CONF_LINE
		"│ %-12s\t"ANSI_COLOR_YELLOW"%20d"ANSI_COLOR_GREEN" │\n" CONF_LINE_BOTTOM"\n"ANSI_COLOR_RESET, 
		"Max Files:",	files, "Max Size:", memory, "Evictions:", 
		server_storage.total_evictions);
		printf("\n\n\x1b[36mFiles stored:"ANSI_COLOR_RESET);
		puts("");
		for (size_t i = 0; i < 2*server_storage.file_limit; i++){
			if(server_storage.storage_table[i] != NULL && !server_storage.storage_table[i]->deleted)
				printf(">> %s\n", server_storage.storage_table[i]->name);
		}
		
}

/**
 * Clean all the heap allocated memory of the storage
 * 
 */
void clean_storage(){
	for(int i = 0; i < 2*server_storage.file_limit; i++){
		if(server_storage.storage_table[i] != NULL){
			clean_attibutes(i);
			free(server_storage.storage_table[i]->data);
			free(server_storage.storage_table[i]->name);
			free(server_storage.storage_table[i]);
		}
	}
	free(server_storage.storage_table);
}

/**
 * Search if an entry is already in the hash table
 * 
 * @param pathname the file to be searched
 * 
 * @returns entry searched or NULL 
 *
 */
static fssFile* search_file(const char* pathname){
	if(!pathname) return NULL;
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

void start_read(fssFile* entry){
	SAFELOCK(entry->order_mutex); // ACQUIRE ORDER
	SAFELOCK(entry->access_mutex); // ACQUIRE ACCESS
	while (entry->writers > 0){
		if(pthread_cond_wait(&entry->go_cond, &entry->access_mutex) != 0){
			fprintf(stderr, "Errore (file %s, linea %d): wait su go_cond non riuscita\n", __FILE__, __LINE__);
			exit(EXIT_FAILURE);
		}
	}
	entry->readers += 1;
	SAFEUNLOCK(file->order_mutex);
	SAFEUNLOCK(file->access_mutex);
}

void stop_read(fssFile* entry){
	SAFELOCK(entry->access_mutex); 
	entry->readers -= 1;
	if(entry->readers == 0){
		if(pthread_cond_signal(&entry->go_cond) < 0){
			fprintf(stderr, "Errore (file %s, linea %d): signal su go_cond non riuscita\n", __FILE__, __LINE__);
			exit(EXIT_FAILURE);
		}
	}
	SAFEUNLOCK(entry->access_mutex);

}

void start_write(fssFile* entry){
	SAFELOCK(entry->order_mutex); // ACQUIRE ORDER
	SAFELOCK(entry->access_mutex); // ACQUIRE ACCESS
	while (entry->readers > 0 || entry->writers > 0){
		if(pthread_cond_wait(&entry->go_cond, &entry->access_mutex) != 0){
			fprintf(stderr, "Errore (file %s, linea %d): wait su go_cond non riuscita\n", __FILE__, __LINE__);
			exit(EXIT_FAILURE);
		}
	}
	entry->writers += 1;
	SAFEUNLOCK(entry->order_mutex); 
	SAFEUNLOCK(entry->access_mutex); 
}

void stop_write(fssFile* entry){
	
	SAFELOCK(entry->access_mutex); 
	entry->writers -= 1;
	if(pthread_cond_signal(&entry->go_cond) < 0){
		fprintf(stderr, "Errore (file %s, linea %d): signal su go_cond non riuscita\n", __FILE__, __LINE__);
		exit(EXIT_FAILURE);
	}
	SAFEUNLOCK(entry->access_mutex);
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