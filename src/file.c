#include "file.h"
#include "zlib.h"

static fssFile* search_file(const char* pathname);
static unsigned int hash_pjw(const void* key);
static int check_memory(unsigned long bytes_to_write, unsigned long old_size, char* caller, victim_queue **queue);
static int check_count();
static void clean_attributes(fssFile *entry, bool close_com);
static void enqueue_victim(fssFile *entry, victim_queue **head);
static void start_read(fssFile* entry, bool unlock_server_mtx);
static void start_write(fssFile* entry, bool unlock_server_mtx);
static void stop_read(fssFile* entry);
static void stop_write(fssFile* entry);
extern int respond_to_client(int com, server_response response);
extern int sendback_client(int com, bool done);
extern void lock_next(char* pathname, bool server_mutex, bool file_mutex);
extern pthread_mutex_t abort_connections_mtx;
extern bool abort_connections;
storage server_storage;
pthread_cond_t start_victim_selector = PTHREAD_COND_INITIALIZER;



void init_table(int max_file_num, int max_size, bool compression, unsigned short compression_level){
	server_storage.file_limit = max_file_num; // nbuckets
	server_storage.size_limit = max_size;
	server_storage.size = 0;
	server_storage.max_size_reached = 0;
	server_storage.max_file_num_reached = 0;
	server_storage.file_count = 0; // nentries
	server_storage.total_evictions = 0;
	server_storage.table_size = server_storage.file_limit * 1.33;
	server_storage.storage_table = (fssFile **) calloc(server_storage.table_size, sizeof(fssFile *));
	server_storage.compression = compression;
	server_storage.compression_level = compression_level;
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
	SAFELOCK(server_storage.storage_access_mtx);
	fssFile* file = search_file(pathname);
	if(!file){
		SAFEUNLOCK(server_storage.storage_access_mtx);
		return -1;
	} 
	
	start_write(file, true);

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
int pop_lock_file_list(char *pathname, int *id, int *com, bool server_mutex, bool file_mutex){
	lock_file_queue *scanner = NULL;
	if(server_mutex) SAFELOCK(server_storage.storage_access_mtx);
	fssFile* file = search_file(pathname);
	if(!file){
		if(server_mutex) SAFEUNLOCK(server_storage.storage_access_mtx);
		return -1;
	} 
	if(file_mutex) start_write(file, server_mutex);
	else if(!file_mutex && server_mutex) SAFEUNLOCK(server_storage.storage_access_mtx);
	scanner = file->lock_waiters;
	if(scanner == NULL){
		if(file_mutex) stop_write(file);
		return -1;
	}
	if(scanner->next == NULL){
		*id = scanner->id;
		*com = scanner->com;
		free(scanner);	
		file->lock_waiters = NULL;
		if(file_mutex) stop_write(file);
		return 0;
	}
	while(scanner->next->next != NULL) scanner = scanner->next;
	*id = scanner->next->id;
	*com = scanner->next->com;
	free(scanner->next);
	scanner->next = NULL;
	if(file_mutex) stop_write(file);
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
	unsigned int index = hash_pjw(pathname);
	bool create_file = (flags & O_CREATE);
	bool lock_file = (flags & O_LOCK);
	SAFELOCK(server_storage.storage_access_mtx);
	fssFile *file = search_file(pathname);
	
	if(file && create_file){
		SAFEUNLOCK(server_storage.storage_access_mtx);
		response->code[0] = FILE_OPERATION_FAILED | FILE_EXISTS;
		return -1;
	}
	
	if(!file && !create_file){
		SAFEUNLOCK(server_storage.storage_access_mtx);
		response->code[0] = FILE_OPERATION_FAILED | FILE_NOT_EXISTS;
		response->code[1] = ENOENT;
		return -1;
	}

	else if(file){
		start_write(file, false);

		if(file->whos_locking != client_id && file->whos_locking > 0){
			stop_write(file);
			SAFEUNLOCK(server_storage.storage_access_mtx);
			response->code[0] = FILE_OPERATION_FAILED | FILE_LOCKED_BY_OTHERS;
			return -1;
		}
		else{
			if(insert_client_file_list(&file->clients_open, client_id) < 0){
				stop_write(file);
				SAFEUNLOCK(server_storage.storage_access_mtx);
				response->code[0] = FILE_ALREADY_OPEN;
				return -1;
			}
			if(lock_file)file->whos_locking = client_id;
		}
		file->use_stat += 1;
		stop_write(file);

	}

	else if(!file){
		check_count();
		file = (fssFile *)calloc(1, sizeof(fssFile));
		CHECKALLOC(file, "Errore inserimento nuovo file");
		insert_client_file_list(&file->clients_open, client_id);
		file->size = 0;
		file->uncompressed_size = 0;
		file->create_time = time(NULL);
		file->last_modified = time(NULL);
		if(pthread_mutex_init(&file->order_mutex, NULL) != 0){
		fprintf(stderr, "Errore di inizializzazione order mutex\n");
		exit(EXIT_FAILURE);
		}
		if(pthread_mutex_init(&file->access_mutex, NULL) != 0){
			fprintf(stderr, "Errore di inizializzazione access mutex\n");
			exit(EXIT_FAILURE);
		}
		if(pthread_cond_init(&file->go_cond, NULL) != 0){
			fprintf(stderr, "Errore di inizializzazione go condition\n");
			exit(EXIT_FAILURE);
		}
		if(lock_file) file->whos_locking = client_id;
		else file->whos_locking = -1;
		file->name = (char *)calloc(strlen(pathname) + 1, sizeof(char));
		CHECKALLOC(file->name, "Errore inserimento nuovo file");
		strncpy(file->name, pathname, strlen(pathname));
		file->use_stat = 16;
		file->readers = 0;
		file->writers = 0;
		file->next = server_storage.storage_table[index];
		server_storage.storage_table[index] = file;
		server_storage.file_count += 1;
		if(server_storage.file_count > server_storage.max_file_num_reached)
			server_storage.max_file_num_reached += 1;
	}
	
	SAFEUNLOCK(server_storage.storage_access_mtx);
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
	SAFELOCK(server_storage.storage_access_mtx);
	fssFile* file = search_file(filename);
	if(!file){
		SAFEUNLOCK(server_storage.storage_access_mtx);
		response->code[0] = FILE_OPERATION_FAILED | FILE_NOT_EXISTS;
		response->code[1] = ENOENT;
		return -1; 
	}
	start_write(file, true);

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

int delete_entry(int id, char *pathname, victim_queue** queue){
	unsigned int index = hash_pjw(pathname);
	fssFile* entry = NULL;
	fssFile* prev = NULL;
	for (entry = server_storage.storage_table[index]; entry; prev = entry, entry = entry->next){
		start_write(entry, false);
		if(strncmp(pathname, entry->name, strlen(pathname)) == 0){
			if(entry->whos_locking == id || id == -2 || entry->whos_locking == -1){
				if(queue && entry->uncompressed_size) enqueue_victim(entry, queue);
				entry->name = (char *) realloc(entry->name, 11);
				strncpy(entry->name, "deleted", 10);
				stop_write(entry);
				server_storage.file_count -= 1;
				server_storage.size -= entry->size;
				
				if(!prev) server_storage.storage_table[index] = entry->next;
				else{
					start_write(prev, false);
					prev->next = entry->next;
					stop_write(prev);
				}
				break;
			}
			stop_write(entry);
			return -1; // EACCESS
		}
		stop_write(entry);
	}
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
	SAFELOCK(server_storage.storage_access_mtx);
	int exit_status = delete_entry(client_id, filename, NULL);
	SAFEUNLOCK(server_storage.storage_access_mtx);

	if(exit_status == -1){
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
	SAFELOCK(server_storage.storage_access_mtx);
	fssFile* file = search_file(filename);
	if(!file){
		SAFEUNLOCK(server_storage.storage_access_mtx);
		response->code[0] = FILE_NOT_EXISTS | FILE_OPERATION_FAILED;
		response->code[1] = ENOENT;
		return -1;
	}
	/* QUI INIZIA IL LETTORE */
	start_read(file, true);
	/* QUI INIZIO A LEGGERE */
	if(check_client_id(file->clients_open, client_id) == -1 && (file->whos_locking == -1 || file->whos_locking == client_id)){
		response->data = (unsigned char *) calloc(file->uncompressed_size, sizeof(unsigned char));
		CHECKALLOC(response->data, "Errore allocazione memoria read_file");
		response->size = file->uncompressed_size;
		if(server_storage.compression) uncompress2(response->data, &response->size, file->data, &file->size);
		else memcpy(response->data, file->data, response->size);
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
 * @returns set the response and returns 0 if successful, -1 if something went wrong, 1 if there are no more files to read
 *
 */
int read_n_file(char **last_file, int client_id, server_response* response){
	unsigned int index = 0;
	SAFELOCK(server_storage.storage_access_mtx);
	fssFile* entry = search_file(*last_file);
	if(!entry){
		while(index < server_storage.table_size && server_storage.storage_table[index] == NULL) index++;
		if(!server_storage.storage_table[index] || index == server_storage.table_size) goto no_more_files;
		entry = server_storage.storage_table[index];
	}
	else{
		if(!entry->next){
			index = hash_pjw(*last_file) + 1;
			while(index < server_storage.table_size && server_storage.storage_table[index] == NULL) index++;
			if(!server_storage.storage_table[index] || index == server_storage.table_size) goto no_more_files;
			entry = server_storage.storage_table[index];
		}
		else entry = entry->next;
	}
	while(true){
		start_read(entry, true);
		if(entry->whos_locking == -1 || entry->whos_locking == client_id){
			response->data = (unsigned char *) calloc(entry->uncompressed_size, sizeof(unsigned char));
			CHECKALLOC(response->data, "Errore allocazione memoria read_file");
			response->size = entry->uncompressed_size;
			if(server_storage.compression) uncompress2(response->data, &response->size, entry->data, &entry->size);
			else memcpy(response->data, entry->data, response->size);
			response->pathlen = strlen(entry->name)+1;
			response->pathname = (char *) calloc(response->pathlen, sizeof(char));
			strncpy(response->pathname, entry->name, response->pathlen-1);
			/* QUI HO FINITO DI LEGGERE ED ESCO */
			stop_read(entry);
			// SAFEUNLOCK(server_storage.storage_access_mtx);
			*last_file = (char *) realloc(*last_file, strlen(entry->name)+1);
			strncpy(*last_file, entry->name, strlen(entry->name));
			response->code[0] = FILE_OPERATION_SUCCESS;
			return 0;	
		}
		stop_read(entry);
		entry = entry->next;
		if(!entry){
			index++;
			SAFELOCK(server_storage.storage_access_mtx);
			while(index < server_storage.table_size && server_storage.storage_table[index] == NULL) index++;
			if(!server_storage.storage_table[index] || index == server_storage.table_size) goto no_more_files;
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
int write_to_file(unsigned char *data, int length, char *pathname, int client_id, server_response *response, victim_queue** victims){
	unsigned char* tmp = NULL;
	unsigned long size = 0;
	SAFELOCK(server_storage.storage_access_mtx);
	fssFile* file = search_file(pathname);
	if(!file){
		SAFEUNLOCK(server_storage.storage_access_mtx);
		response->code[1] = ENOENT;
		response->code[0] = FILE_OPERATION_FAILED | FILE_NOT_EXISTS;
		return -1;
	}
	/* QUI INIZIA LO SCRITTORE */
	start_write(file, false); 

	/* QUI SI SCRIVE */
	if(file->whos_locking == client_id && check_client_id(file->clients_open, client_id) < 0 && file->data == NULL){
		if(server_storage.compression){
			tmp = (unsigned char *) calloc(length+20, sizeof(unsigned char)); // zlib needs some bytes to store header and trailer, so if the input data is small and compress does not have effect, this prevents a seg fault 
			CHECKALLOC(tmp, "Errore allocazione write_to_file");
			size = length + 20;
			compress2(tmp, &size, data, length, server_storage.compression_level);
		}
		else size = length;
		if(check_memory(size, 0, pathname, victims) < 0){
			response->code[1] = EFBIG;
			response->code[0] = FILE_OPERATION_FAILED;
			stop_write(file);
			SAFEUNLOCK(server_storage.storage_access_mtx);
			free(tmp);
			return -1;
		}
		if((*victims)) response->has_victim = 0x01;
		
		file->data = (unsigned char *) calloc(size, sizeof(unsigned char));
		CHECKALLOC(file->data, "Errore allocazione write_to_file");
		file->size = size;
		memcpy(file->data, server_storage.compression ? tmp : data, size);
		file->uncompressed_size = length;
		file->use_stat += 1;
		file->last_modified = time(NULL);
		
		/* QUI HO FINITO DI SCRIVERE ED ESCO */
		stop_write(file);
		server_storage.size += size;
		if(server_storage.size > server_storage.max_size_reached)  server_storage.max_size_reached = server_storage.size;
		pthread_cond_signal(&start_victim_selector);
		SAFEUNLOCK(server_storage.storage_access_mtx);
		response->code[0] = FILE_OPERATION_SUCCESS;
		free(tmp);
		return 0;
	}
	stop_write(file);
	SAFEUNLOCK(server_storage.storage_access_mtx);
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
int append_to_file(unsigned char* new_data, int new_data_size, char *pathname, int client_id, server_response *response, victim_queue** victims){
	unsigned char* tmp = NULL;
	unsigned char* uncompressed_buffer = NULL;

	size_t new_size = 0, bytes_to_append = 0;
	SAFELOCK(server_storage.storage_access_mtx);
	fssFile* file = search_file(pathname);
	if(!file){
		SAFEUNLOCK(server_storage.storage_access_mtx);

		response->code[0] = FILE_OPERATION_FAILED | FILE_NOT_EXISTS;
		response->code[1] = ENOENT;
		return -1;
	}
	
	/* QUI INIZIA LO SCRITTORE */
	start_write(file, false);

	if(check_client_id(file->clients_open, client_id) == -1 && (file->whos_locking == -1 || file->whos_locking == client_id)){
		if(server_storage.compression){
			new_size = file->uncompressed_size + new_data_size + 20;
			uncompressed_buffer = (unsigned char *) calloc(file->uncompressed_size + new_data_size, sizeof(unsigned char)); 
			CHECKALLOC(uncompressed_buffer, "Errore allocazione append_to_file");
			tmp = (unsigned char *) calloc(new_size, sizeof(unsigned char));
			CHECKALLOC(tmp, "Errore allocazione append_to_file");
			uncompress2(uncompressed_buffer, &file->uncompressed_size, file->data, &file->size);
			memcpy(uncompressed_buffer + file->uncompressed_size, new_data, new_data_size);
			compress2(tmp, &new_size, uncompressed_buffer, file->uncompressed_size + new_data_size, server_storage.compression_level);
			bytes_to_append = new_size - file->size;
		}
		else{
			bytes_to_append = new_data_size;
			new_size = file->size + bytes_to_append;
		} 
		
		
		if(check_memory(bytes_to_append, file->size, pathname, victims) < 0){ // 22/07 SPOSTATO QUI, ERA DOPO "if(!file)"
			if(server_storage.compression){
				free(tmp);
				free(uncompressed_buffer);
			}
			response->code[1] = EFBIG;
			response->code[0] = FILE_OPERATION_FAILED;
			stop_write(file);
			SAFEUNLOCK(server_storage.storage_access_mtx);
			return -1;
		}
		if((*victims)) response->has_victim = 0x01; 

		file->data = (unsigned char *) realloc(file->data, new_size);
		CHECKALLOC(file->data, "Errore allocazione append_to_file");
		if(server_storage.compression) memcpy(file->data, tmp, new_size);
		else memcpy(file->data + file->size, new_data, new_data_size);
		
		file->size = new_size;
		file->uncompressed_size += new_data_size;

		file->use_stat += 1;
		file->last_modified= time(NULL);

		stop_write(file);
		server_storage.size += bytes_to_append;
		if(server_storage.size > server_storage.max_size_reached)  server_storage.max_size_reached = server_storage.size;
		pthread_cond_signal(&start_victim_selector);
		SAFEUNLOCK(server_storage.storage_access_mtx);
		response->code[0] = FILE_OPERATION_SUCCESS;
		if(server_storage.compression){
			free(tmp);
			free(uncompressed_buffer);
		}
		return 0;
	}
	if(file->whos_locking == -1 || file->whos_locking == client_id){
		stop_write(file);
		SAFEUNLOCK(server_storage.storage_access_mtx);
		response->code[1] = EBUSY;
		response->code[0] = FILE_OPERATION_FAILED | FILE_LOCKED_BY_OTHERS;
		return -1;
	}
	stop_write(file);
	SAFEUNLOCK(server_storage.storage_access_mtx);
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
int lock_file(char *pathname, int client_id, bool server_mutex, bool file_mutex,  server_response *response){
	if(server_mutex) SAFELOCK(server_storage.storage_access_mtx);
	fssFile* file = search_file(pathname);
	if(!file){
		if(server_mutex) SAFEUNLOCK(server_storage.storage_access_mtx);
		response->code[0] = FILE_NOT_EXISTS | FILE_OPERATION_FAILED;
		response->code[1] = ENOENT;
		return -1;
	}
	/* QUI INIZIA LO SCRITTORE */
	if(file_mutex) start_write(file, server_mutex);
	else if(!file_mutex && server_mutex) SAFEUNLOCK(server_storage.storage_access_mtx);

	if(file->whos_locking == -1){
		file->whos_locking = client_id;
		file->use_stat += 1;
		if(file_mutex) stop_write(file);
		response->code[0] = FILE_OPERATION_SUCCESS;
		return 0;
	}
	else if(file->whos_locking == client_id){
		if(file_mutex) stop_write(file);
		response->code[0] = FILE_ALREADY_LOCKED | FILE_OPERATION_FAILED;
		response->code[1] = EINVAL;
		return -1;
	}
	if(file_mutex) stop_write(file);
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
	SAFELOCK(server_storage.storage_access_mtx);
	fssFile* file = search_file(pathname);
	if(!file){
		SAFEUNLOCK(server_storage.storage_access_mtx);
		response->code[0] = FILE_NOT_EXISTS | FILE_OPERATION_FAILED;
		response->code[1] = ENOENT;
		return -1;
	}
	/* QUI INIZIA LO SCRITTORE */
	start_write(file, true);

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
	printf("\033[2A");
	SAFELOCK(server_storage.storage_access_mtx);
	snprintf(memory, 20, "%lu/%lu", server_storage.size, server_storage.size_limit); 
	snprintf(files, 20, "%u/%u", server_storage.file_count, server_storage.file_limit); 
	SAFEUNLOCK(server_storage.storage_access_mtx);
	printf(ANSI_COLOR_CYAN"»»» %-20s  "ANSI_COLOR_YELLOW"%20s"ANSI_COLOR_CYAN" \n"
			"»»» %-20s  "ANSI_COLOR_YELLOW"%20s"ANSI_COLOR_GREEN" "ANSI_COLOR_RESET_N, "Memory Used:",
			memory, "Files in Memory:", files);
	
}

void print_summary(){
	char memory[20];
	char files[20];
	fssFile* file = NULL;
	puts("\n");
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
			start_read(file, false);
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
	
	for (entry = server_storage.storage_table[index]; entry; entry = entry->next){
		start_read(entry, false);
		if(strncmp(pathname, entry->name, strlen(pathname)) == 0){
			stop_read(entry);
			return entry; // Found
		}
		stop_read(entry);
	}

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

static void start_read(fssFile* entry, bool unlock_server_mtx){
	SAFELOCK(entry->order_mutex); // ACQUIRE ORDER
	SAFELOCK(entry->access_mutex); // ACQUIRE ACCESS
	if(unlock_server_mtx) SAFEUNLOCK(server_storage.storage_access_mtx);
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

static void stop_read(fssFile* entry){
	SAFELOCK(entry->access_mutex); 
	entry->readers -= 1;
	if(entry->readers == 0){
		if(pthread_cond_broadcast(&entry->go_cond) < 0){
			fprintf(stderr, "Errore (file %s, linea %d): signal su go_cond non riuscita\n", __FILE__, __LINE__);
			exit(EXIT_FAILURE);
		}
	}
	SAFEUNLOCK(entry->access_mutex);

}

static void start_write(fssFile* entry, bool unlock_server_mtx){
	SAFELOCK(entry->order_mutex); // ACQUIRE ORDER
	SAFELOCK(entry->access_mutex); // ACQUIRE ACCESS
	if(unlock_server_mtx) SAFEUNLOCK(server_storage.storage_access_mtx);
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

static void stop_write(fssFile* entry){
	
	SAFELOCK(entry->access_mutex); 
	entry->writers -= 1;
	if(pthread_cond_broadcast(&entry->go_cond) < 0){
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

static void enqueue_victim(fssFile *entry, victim_queue **head){
	victim_queue* new = (victim_queue *) malloc(sizeof(victim_queue));
	memset(new, 0, sizeof(victim_queue));
	new->victim.pathlen = strlen(entry->name) + 1;
	new->victim.pathname = (char *) calloc(new->victim.pathlen, sizeof(char));
	strncpy(new->victim.pathname, entry->name, strlen(entry->name));
	new->victim.size = entry->uncompressed_size;
	new->victim.data = (unsigned char *) calloc(new->victim.size , sizeof(unsigned char));
	if(server_storage.compression) uncompress2(new->victim.data, &new->victim.size, entry->data, &entry->size);
	else memcpy(new->victim.data, entry->data, entry->size);
	new->next = (*head);
	(*head) = new;
}

static int select_victim(char* caller, int files_to_delete, unsigned long memory_to_free, victim_queue **queue) {
	victim* victims = NULL;
	fssFile* entry = NULL;
	int counter = 0, j = 0;
	unsigned long memory_freed = 0;
	victims = (victim *) calloc(server_storage.file_count, sizeof(victim));
	server_storage.total_evictions += 1;
	for (size_t i = 0; i < server_storage.table_size; i++){
		entry = server_storage.storage_table[i];
		while(entry){
			if(caller && strncmp(entry->name, caller, strlen(entry->name)) == 0){
				entry = entry->next;
				continue;
			} 
			start_read(entry, false);
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
	
	
	qsort(victims, counter, sizeof(victim), compare);
	if(files_to_delete){
		delete_entry(-2, victims[0].pathname, queue);
		for (int i = 0; i < counter; i++)
			free(victims[i].pathname);
		
		free(victims);
		return 0;
	}
	while(j < counter && memory_freed < memory_to_free){
		delete_entry(-2, victims[j].pathname, queue);
		memory_freed += victims[j].size;
		j++;
	}

	for (int i = 0; i < counter; i++)
		free(victims[i].pathname);
		
	free(victims);
	return 0;
}

/**
 * Check whether there's enough space in memory or not
 * 
 * @param bytes_to_write bytes to write into the file
 * @param old_size size of the original file
 * @param caller file that calls the check memory function
 * @param server_mutex_lock flag to toggle the lock on whole storage while checking
 * @param queue queue of the evicted files
 * 
 * @returns 0 if the operation is successful, -1 if the file is too big or there are no locked files to remove
 *
 */
static int check_memory(unsigned long bytes_to_write, unsigned long old_size, char* caller, victim_queue **queue){
	unsigned long size_used = 0; // never unlock storage mtx before finishing select victim, this must be performed atomically
	int retval = 0;
	// SAFELOCK(server_storage.storage_access_mtx);
	size_used = server_storage.size;
	if(bytes_to_write + old_size > server_storage.size_limit){
		// SAFEUNLOCK(server_storage.storage_access_mtx);
		return -1;
	}
		
	if(size_used + bytes_to_write <= server_storage.size_limit){
		// SAFEUNLOCK(server_storage.storage_access_mtx);
		return 0;
	} 
	retval = select_victim(caller, 0, (bytes_to_write + size_used) - server_storage.size_limit, queue);
	// SAFEUNLOCK(server_storage.storage_access_mtx);
	return retval;
}

static int check_count(){
	if(server_storage.file_count + 1 <= server_storage.file_limit)
		return 0;
	
	return select_victim(NULL, 1, 0, NULL);
}

void* use_stat_update(void *args){
	fssFile* file = NULL;
	if(pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL) < 0) exit(EXIT_FAILURE);
	while(true){
		SAFELOCK(server_storage.storage_access_mtx);
		pthread_cond_wait(&start_victim_selector, &server_storage.storage_access_mtx);
		for(int i = 0; i < server_storage.table_size; i++){
			
			if(server_storage.storage_table[i] == NULL) continue;

			file = server_storage.storage_table[i];
			while(file){
				start_write(file, false);
				if(file->use_stat != 0)
					file->use_stat -= 1;
				if(file->use_stat == 0 && (file->last_modified - time(NULL)) > 120){
					file->whos_locking = -1; // Automatic unlock if the file is not used for more than 2 minutes
					lock_next(file->name, false, false); // Then next client in lock queue acquires lock
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
