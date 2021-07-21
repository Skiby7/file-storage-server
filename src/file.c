#include "file.h"

static int create_new_entry(int id, char *pathname, int flags);
static fssFile* search_file(const char* pathname);
static unsigned int hash_pjw(const void* key);
static int check_memory(unsigned long new_size, unsigned long old_size, char* caller, bool server_mutex_lock);
static int check_count(bool server_mutex_lock);
static void clean_attributes(fssFile *entry, bool close_com);
void start_read(fssFile* entry);
void start_write(fssFile* entry);
void stop_read(fssFile* entry);
void stop_write(fssFile* entry);
extern int respond_to_client(int com, server_response response);
extern int sendback_client(int com, bool done);
extern void lock_next(char* pathname, bool mutex_write);
extern pthread_mutex_t abort_connections_mtx;
extern bool abort_connections;
storage server_storage;
pthread_cond_t start_victim_selector = PTHREAD_COND_INITIALIZER;



void init_table(int max_file_num, int max_size){
	server_storage.file_limit = max_file_num; // nbuckets
	server_storage.size_limit = max_size;
	server_storage.size = 0;
	server_storage.max_size_reached = 0;
	server_storage.max_file_num_reached = 0;
	server_storage.file_count = 0; // nentries
	server_storage.total_evictions = 0;
	server_storage.table_size = server_storage.file_limit * 1.33;
	server_storage.storage_table = (fssFile **) calloc(server_storage.table_size, sizeof(fssFile *));
	pthread_mutex_init(&server_storage.storage_access_mtx, NULL);
}


static int check_client_id(open_file_client_list *head, int id){
	while(head != NULL){
		if(head->id == id) return -1;
		head = head->next;
	}
	return 0;
}


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
 * @param pathname pathname of the file
 * @param id the id of the client to add
 * @param com the file descriptor of the client
 * 
 * @returns 0 if successful, -1 the client is already in the list 
 *
 */
int insert_lock_file_list(char *pathname, int id, int com){
	fssFile* file = search_file(pathname);
	if(!file) return -1;
	
	start_write(file);

	if(check_client_id_lock(file->lock_waiters, id) == -1){
		stop_write(file);
		return -1;
	}
	lock_file_queue *new = (lock_file_queue *) malloc(sizeof(lock_file_queue));
	new->id = id;
	new->com = com;
	new->next = file->lock_waiters;
	file->lock_waiters = new;	
	stop_write(file);
	return 0;
}

/**
 * Pop client id in the lock_file_queue of the filename
 * 
 * @param pathname pathname of the file
 * @param id pointer to return the id of the client popped
 * @param com pointer to return the com of the client popped
 * 
 * @returns 0 if successful, -1 the client is not in the queue 
 *
 */
int pop_lock_file_list(char *pathname, int *id, int *com){
	fssFile* file = search_file(pathname);
	lock_file_queue *scanner = NULL;
	if(!file) return -1;
	start_write(file);
	scanner = file->lock_waiters;
	if(scanner == NULL){
		stop_write(file);
		return -1;
	}
	if(scanner->next == NULL){
		*id = scanner->id;
		*com = scanner->com;
		free(scanner);	
		file->lock_waiters = NULL;
		stop_write(file);
		return 0;
	}
	while(scanner->next->next != NULL) scanner = scanner->next;
	*id = scanner->next->id;
	*com = scanner->next->com;
	free(scanner->next);
	scanner->next = NULL;
	stop_write(file);
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


static int create_new_entry(int id, char *pathname, int flags){
	unsigned int index = hash_pjw(pathname);
	fssFile *new_entry = NULL;
	bool create_file = (flags & O_CREATE);
	bool lock_file = (flags & O_LOCK);
	SAFELOCK(server_storage.storage_access_mtx);
	
	for (new_entry = server_storage.storage_table[index]; new_entry; new_entry = new_entry->next){
		start_read(new_entry);
		if(strncmp(pathname, new_entry->name, strlen(pathname)) == 0){
			stop_read(new_entry);
			SAFEUNLOCK(server_storage.storage_access_mtx);
			// printf("PATHNAME FOUND %s CREATE_FILE %d\n", new_entry->name, create_file);
			if(create_file) return -1; // Found
			else return 1; 

		}
		stop_read(new_entry);
	}
	if(!create_file){
		SAFEUNLOCK(server_storage.storage_access_mtx);
		return -1;
	} 
	check_count(false);
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
	
	if(lock_file) new_entry->whos_locking = id;
	else new_entry->whos_locking = -1;
	new_entry->name = (char *)calloc(strlen(pathname) + 1, sizeof(char));
	CHECKALLOC(new_entry->name, "Errore inserimento nuovo file");
	strncpy(new_entry->name, pathname, strlen(pathname));
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
 * @param pathname pathname of the file
 * @param flags O_CREATE to create the file, O_LOCK to lock the file
 * @param client_id id of the client opening the file
 * @param response pointer to the server_response
 * 
 * @returns set the response and returns 0 if successful, -1 if something went wrong
 *
 */
int open_file(char *pathname, int flags, int client_id, server_response *response){
	int create_ret = create_new_entry(client_id, pathname, flags);
	bool create_file = (flags & O_CREATE);
	bool lock_file = (flags & O_LOCK);
	fssFile *file = NULL;

	// printf(ANSI_COLOR_RED"Filename: %s, create_flag %d, lock_flag %d, exists %d\n"ANSI_COLOR_RESET, filename, create_file, lock_file, file_exists);
	
	if(create_ret == -1 && create_file){
		response->code[0] = FILE_OPERATION_FAILED | FILE_EXISTS;
		return -1;
	}
	
	if(create_ret == -1 && !create_file){
		response->code[0] = FILE_OPERATION_FAILED | FILE_NOT_EXISTS;
		response->code[1] = ENOENT;
		return -1;
	}
	
	else if(create_ret == 1){
		file = search_file(pathname);
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

int delete_entry(int id, char *pathname, bool server_mutex_lock){
	unsigned int index = hash_pjw(pathname);
	fssFile* entry = NULL;
	fssFile* prev = NULL;
	if(server_mutex_lock) SAFELOCK(server_storage.storage_access_mtx);
	for (entry = server_storage.storage_table[index]; entry; prev = entry, entry = entry->next){
		start_write(entry);
		if(strncmp(pathname, entry->name, strlen(pathname)) == 0){
			if(entry->whos_locking == id || id == -2 || entry->whos_locking == -1){
				entry->name = (char *) realloc(entry->name, 11);
				strncpy(entry->name, "deleted", 10);
				stop_write(entry);
				server_storage.file_count -= 1;
				server_storage.size -= entry->size;
				
				if(!prev) server_storage.storage_table[index] = entry->next;
				else{
					start_write(prev);
					prev->next = entry->next;
					stop_write(prev);
				}
				break;
			}
			stop_write(entry);
			if(server_mutex_lock) SAFEUNLOCK(server_storage.storage_access_mtx);
			return -1; // EACCESS
		}
		stop_write(entry);
	}
	if(server_mutex_lock) SAFEUNLOCK(server_storage.storage_access_mtx);
	if(!entry){
		return -2; // ENOENT
	}
	free(entry->name);
	if(entry->data) free(entry->data);
	CHECKSCEXIT(pthread_mutex_destroy(&entry->access_mutex), false, "Errore pthread_mutex_destroy");
	CHECKSCEXIT(pthread_mutex_destroy(&entry->order_mutex), false, "Errore pthread_mutex_destroy");
	CHECKSCEXIT(pthread_cond_destroy(&entry->go_cond), false, "Errore pthread_cond_destroy");
	clean_attributes(entry, false);
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
	int exit_status = delete_entry(client_id, filename, true);
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
	unsigned int index = 0;
	fssFile* entry = search_file(*last_file);
	SAFELOCK(server_storage.storage_access_mtx);
	if(!entry){
		while(index < server_storage.table_size && server_storage.storage_table[index] == NULL) index++;
		if(!server_storage.storage_table[index]) goto no_more_files;
		entry = server_storage.storage_table[index];
	}
	else{
		index = hash_pjw(*last_file) + 1;
		entry = entry->next;
		if(!entry){
			while(index < server_storage.table_size && server_storage.storage_table[index] == NULL) index++;
			if(!server_storage.storage_table[index]) goto no_more_files;
			entry = server_storage.storage_table[index];
		}
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
		entry = entry->next;
		if(!entry){
			index++;
			while(index < server_storage.table_size && server_storage.storage_table[index] == NULL) index++;
			if(!server_storage.storage_table[index]) goto no_more_files;
			entry = server_storage.storage_table[index];
		}
	}
	
no_more_files:
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
	if(check_memory(length, 0, pathname, true) < 0){
		response->code[1] = EFBIG;
		response->code[0] = FILE_OPERATION_FAILED;
		return -1;
	}
	/* QUI INIZIA LO SCRITTORE */
	start_write(file);

	/* QUI SI SCRIVE */
	if(file->whos_locking == client_id && check_client_id(file->clients_open, client_id) < 0 && file->data == NULL){
		
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
		pthread_cond_signal(&start_victim_selector);
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
	if(check_memory(new_data_size, old_size, pathname, true) < 0){
		response->code[0] = FILE_OPERATION_FAILED;
		response->code[1] = EFBIG;
		return -1;
	}
	/* QUI INIZIA LO SCRITTORE */
	start_write(file);
	if(check_client_id(file->clients_open, client_id) == -1 && (file->whos_locking == -1 || file->whos_locking == client_id)){

		file->data = (unsigned char *) realloc(file->data, new_data_size + old_size);
		CHECKALLOC(file->data, "Errore allocazione append_to_file");
		memcpy(file->data + old_size, new_data, new_data_size);
		// for (size_t i = old_size, j = 0; j < new_data_size; i++, j++)
		// 	server_storage.storage_table[file_index]->data[i] = new_data[j];

		file->size = new_data_size + old_size;
		file->use_stat += 1;
		file->last_modified= time(NULL);

		stop_write(file);
		SAFELOCK(server_storage.storage_access_mtx);
		server_storage.size += new_data_size;
		if(server_storage.size > server_storage.max_size_reached)  server_storage.max_size_reached = server_storage.size;
		pthread_cond_signal(&start_victim_selector);
		SAFEUNLOCK(server_storage.storage_access_mtx);
		response->code[0] = FILE_OPERATION_SUCCESS;
		return 0;
	}
	if(file->whos_locking == -1 || file->whos_locking == client_id){
		stop_write(file);
		response->code[1] = EACCES;
		response->code[0] = FILE_OPERATION_FAILED | FILE_LOCKED_BY_OTHERS;
		return -1;
	}
	stop_write(file);
	response->code[1] = EACCES;
	response->code[0] = FILE_OPERATION_FAILED;
	return -1;

}

/**
 * Lock the file identified by filename
 * 
 * @param pathname pathname of the file
 * @param client_id id of the client reading the file
 * @param response pointer to the server_response
 * 
 * @returns set the response and returns 0 if successful, -1 if something went wrong
 *
 */
int lock_file(char *pathname, int client_id, bool mutex_write, server_response *response){
	fssFile* file = search_file(pathname);
	if(!file){
		response->code[0] = FILE_NOT_EXISTS | FILE_OPERATION_FAILED;
		response->code[1] = ENOENT;
		return -1;
	}
	/* QUI INIZIA LO SCRITTORE */
	if(mutex_write) start_write(file);
	if(file->whos_locking == -1){
		file->whos_locking = client_id;
		file->use_stat += 1;
		if(mutex_write) stop_write(file);
		response->code[0] = FILE_OPERATION_SUCCESS;
		return 0;
	}
	else if(file->whos_locking == client_id){
		if(mutex_write) stop_write(file);
		response->code[0] = FILE_ALREADY_LOCKED | FILE_OPERATION_FAILED;
		response->code[1] = EINVAL;
		return -1;
	}
	if(mutex_write) stop_write(file);
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
int unlock_file(char *pathname, int client_id, server_response *response){
	fssFile* file = search_file(pathname);
	if(!file){
		response->code[0] = FILE_NOT_EXISTS | FILE_OPERATION_FAILED;
		response->code[1] = ENOENT;
		return -1;
	}
	/* QUI INIZIA LO SCRITTORE */
	start_write(file);
	if(file->whos_locking == client_id){
		file->whos_locking = -1;
		stop_write(file);
		response->code[0] = FILE_OPERATION_SUCCESS;
		return 0;
	}
	else if(file->whos_locking != client_id){
		stop_write(file);
		response->code[0] = FILE_OPERATION_FAILED | FILE_LOCKED_BY_OTHERS;
		response->code[1] = EBUSY;
		return -1;
	}
	stop_write(file);
	response->code[0] = FILE_NOT_LOCKED | FILE_OPERATION_FAILED;
	return -1;
}


void print_storage_info(){
	char memory[20];
	char files[20];
	SAFELOCK(server_storage.storage_access_mtx);
	snprintf(memory, 20, "%lu/%lu", server_storage.size, server_storage.size_limit); 
	snprintf(files, 20, "%u/%u", server_storage.file_count, server_storage.file_limit); 
	SAFEUNLOCK(server_storage.storage_access_mtx);
	printf(ANSI_COLOR_CYAN"»»» %-12s\t"ANSI_COLOR_YELLOW"%20s"ANSI_COLOR_CYAN" \n"
			"»»» %-12s\t"ANSI_COLOR_YELLOW"%20s"ANSI_COLOR_GREEN" \n" ANSI_COLOR_RESET, "Memory:",
			memory, "Files:", files);
}

void print_summary(){
	char memory[20];
	char files[20];
	fssFile* file = NULL;
	snprintf(memory, 20, "%lu/%lu", server_storage.max_size_reached, server_storage.size_limit); 
	snprintf(files, 20, "%u/%u", server_storage.max_file_num_reached, server_storage.file_limit); 
	printf(ANSI_COLOR_GREEN CONF_LINE_TOP
		"│ %-12s\t"ANSI_COLOR_YELLOW"%20s"ANSI_COLOR_GREEN" │\n" CONF_LINE
		"│ %-12s\t"ANSI_COLOR_YELLOW"%20s"ANSI_COLOR_GREEN" │\n" CONF_LINE
		"│ %-12s\t"ANSI_COLOR_YELLOW"%20d"ANSI_COLOR_GREEN" │\n" CONF_LINE_BOTTOM"\n"ANSI_COLOR_RESET, 
		"Max Files:",	files, "Max Size:", memory, "Evictions:", 
		server_storage.total_evictions);
		printf("\n\x1b[36mFile rimasti in memoria :"ANSI_COLOR_RESET_N);
		for (size_t i = 0; i < server_storage.table_size; i++){
			if(server_storage.storage_table[i] != NULL){
				file = server_storage.storage_table[i];
				while(file){
					printf(">> %s at %lu\n", file->name, i);
					file = file->next;
				}
			}

				
		}
}
void print_storage(){
	fssFile* file = NULL;
	SAFELOCK(server_storage.storage_access_mtx);
	for(int i = 0; i < server_storage.table_size; i++){
		if(server_storage.storage_table[i] == NULL) continue;
				
			
		file = server_storage.storage_table[i];
		while(file){
			start_read(file);
			printf("-----\nPathname: %s\nSize: %ld\nData: %s\nUse_stat: %u\nWhos_locking: %d\n-----\n\n", 
					file->name, file->size, file->data ? "Has data" : "NULL", file->use_stat, file->whos_locking);
			stop_read(file);
			file = file->next;
		}
			
	}
	SAFEUNLOCK(server_storage.storage_access_mtx);
}

void destroy_table_entry(fssFile* entry){
	if(!entry) return;
	destroy_table_entry(entry->next);
	clean_attributes(entry, true);
	if(entry->data) free(entry->data);
	free(entry->name);
	pthread_mutex_destroy(&entry->access_mutex);
	pthread_mutex_destroy(&entry->order_mutex);
	pthread_cond_destroy(&entry->go_cond);
	if(entry->next) free(entry->next);
}

/**
 * Clean all the heap allocated memory of the storage
 * 
 */
void clean_storage(){
	
	for(int i = 0; i < server_storage.table_size; i++){
		if(server_storage.storage_table[i]){
			destroy_table_entry(server_storage.storage_table[i]);
			free(server_storage.storage_table[i]);
		}
	}
	pthread_mutex_destroy(&server_storage.storage_access_mtx);
	pthread_cond_destroy(&start_victim_selector);
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
	unsigned int index = hash_pjw(pathname);
	fssFile* entry = NULL;
	SAFELOCK(server_storage.storage_access_mtx);
	
	for (entry = server_storage.storage_table[index]; entry; entry = entry->next){
		start_read(entry);
		if(strncmp(pathname, entry->name, strlen(pathname)) == 0){
			stop_read(entry);
			SAFEUNLOCK(server_storage.storage_access_mtx);
			return entry; // Found
		}
		stop_read(entry);
	}

	SAFEUNLOCK(server_storage.storage_access_mtx);
	return NULL;
}

static unsigned int hash_pjw(const void* key){
    char *datum = (char *)key;
    unsigned int hash_value, i;

    if(!datum) return 0;

    for (hash_value = 0; *datum; ++datum) {
        hash_value = (hash_value << ONE_EIGHTH) + *datum;
        if ((i = hash_value & HIGH_BITS) != 0)
            hash_value = (hash_value ^ (i >> THREE_QUARTERS)) & ~HIGH_BITS;
    }
    return (hash_value) % server_storage.table_size;
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
	SAFEUNLOCK(entry->order_mutex);
	SAFEUNLOCK(entry->access_mutex);
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

static int select_victim(char* caller, int files_to_delete, unsigned long memory_to_free, bool server_mutex_lock) {
	victim* victims = NULL;
	fssFile* entry = NULL;
	int counter = 0, j = 0;
	unsigned long memory_freed = 0;
	if(server_mutex_lock) SAFELOCK(server_storage.storage_access_mtx);
	victims = (victim *) calloc(server_storage.file_count, sizeof(victim));
	server_storage.total_evictions += 1;
	for (size_t i = 0; i < server_storage.table_size; i++){
		entry = server_storage.storage_table[i];
		while(entry){
			if(caller && strncmp(entry->name, caller, strlen(entry->name)) == 0) continue;
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
			entry = entry->next;
		}
	}
	if(server_mutex_lock) SAFEUNLOCK(server_storage.storage_access_mtx);
	
	
	qsort(victims, counter, sizeof(victim), compare);
	if(files_to_delete){
		delete_entry(-2, victims[0].pathname, false);
		for (int i = 0; i < counter; i++)
			free(victims[i].pathname);
		
		free(victims);
		return 0;
	}
	while(j < counter && memory_freed < memory_to_free){
		delete_entry(-2, victims[0].pathname, false);
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
static int check_memory(unsigned long new_size, unsigned long old_size, char* caller, bool server_mutex_lock){
	unsigned long size_used = 0;
	SAFELOCK(server_storage.storage_access_mtx);
	size_used = server_storage.size;
	SAFEUNLOCK(server_storage.storage_access_mtx);
	if(new_size + old_size > server_storage.size_limit)
		return -1;

	
	if(size_used + new_size <= server_storage.size_limit) return 0;
	return select_victim(caller, 0, (new_size + size_used) - server_storage.size_limit, server_mutex_lock);
}

static int check_count(bool server_mutex_lock){
	if(server_mutex_lock) SAFELOCK(server_storage.storage_access_mtx);
	if(server_storage.file_count + 1 <= server_storage.file_limit){
		if(server_mutex_lock) SAFEUNLOCK(server_storage.storage_access_mtx);
		return 0;
	}
	if(server_mutex_lock) SAFEUNLOCK(server_storage.storage_access_mtx);
	return select_victim(NULL, 1, 0, server_mutex_lock);
}

void* use_stat_update(void *args){
	fssFile* file = NULL;
	while(true){
		SAFELOCK(server_storage.storage_access_mtx);
		pthread_cond_wait(&start_victim_selector, &server_storage.storage_access_mtx);
		SAFELOCK(abort_connections_mtx);
		if(abort_connections){
			SAFEUNLOCK(abort_connections_mtx);
			SAFEUNLOCK(server_storage.storage_access_mtx);
			return (void *) 0;
		}
		SAFEUNLOCK(abort_connections_mtx);
		for(int i = 0; i < server_storage.table_size; i++){
			if(server_storage.storage_table[i] == NULL) continue;

			file = server_storage.storage_table[i];
			while(file){
				start_write(file);
				if(file->use_stat != 0)
					file->use_stat -= 1;
				if(file->use_stat == 0 && (file->last_modified - time(NULL)) > 120){
					file->whos_locking = -1; // Automatic unlock if the file is not used for more than 2 minutes
					lock_next(file->name, false); // Then next client in lock queue acquires lock
				} 
				stop_write(file);
				file = file->next;
			}
			
		}
		SAFEUNLOCK(server_storage.storage_access_mtx);
	}
}


void clean_attributes(fssFile *entry, bool close_com){
	open_file_client_list *befree = NULL;
	lock_file_queue *befree1 = NULL;
	server_response response;
	while (entry->clients_open != NULL){
		befree = entry->clients_open;
		entry->clients_open = entry->clients_open->next;
		free(befree);
	}
	memset(&response, 0, sizeof response);
	response.code[0] = FILE_OPERATION_FAILED | FILE_NOT_EXISTS;
	response.code[1] = ENOENT;

	while (entry->lock_waiters != NULL){
		response.pathlen = strlen(entry->name) + 1;
		response.pathname = (char *) calloc(response.pathlen, sizeof(char));
		strcpy(response.pathname, entry->name);
		respond_to_client(entry->lock_waiters->com, response);
		if(close_com) close(entry->lock_waiters->com);
		else sendback_client(entry->lock_waiters->com, false);
		befree1 = entry->lock_waiters;
		entry->lock_waiters = entry->lock_waiters->next;
		free(befree1);
		free(response.pathname);
	}
}
