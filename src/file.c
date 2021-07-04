#include "file.h"


storage server_storage;
pthread_mutex_t storage_access_mtx = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t start_LFU_selector = PTHREAD_COND_INITIALIZER;
static void init_file(int id, char *filename, bool locked);
static unsigned int search_file(const char* pathname);
static unsigned int get_free_index(const char* pathname);
static unsigned int hash_val(const void* key, unsigned int i, unsigned int max_len, unsigned int key_len);
void start_read(int file_index);
void start_write(int file_index);
void stop_read(int file_index);
void stop_write(int file_index);
extern int respond_to_client(int com, server_response response);
extern int sendback_client(int com, bool done);
extern void lock_next(char* pathname, bool mutex_write);

static int compare(const void *a, const void *b) {
	victim a1 = *(victim *)a, b1 = *(victim *)b; 
	time_t now = time(NULL);
	if((a1.use_stat - b1.use_stat) != 0)
		return a1.use_stat - b1.use_stat; // sort by use stat

	else return (now - a1.last_modified) - (now - b1.last_modified); // if use stat is the same, sort by age
}

void clean_attibutes(int index){
	open_file_client_list *befree = NULL;
	lock_file_queue *befree1 = NULL;
	if(server_storage.storage_table[index]->clients_open != NULL){
		while (server_storage.storage_table[index]->clients_open != NULL){
			befree = server_storage.storage_table[index]->clients_open;
			server_storage.storage_table[index]->clients_open = server_storage.storage_table[index]->clients_open->next;
			free(befree);
		}
	}
	if(server_storage.storage_table[index]->lock_waiters != NULL){
		while (server_storage.storage_table[index]->lock_waiters != NULL){
			close(server_storage.storage_table[index]->lock_waiters->com);
			befree1 = server_storage.storage_table[index]->lock_waiters;
			server_storage.storage_table[index]->lock_waiters = server_storage.storage_table[index]->lock_waiters->next;
			free(befree1);
		}
	}
}

void empty_lock_queue(index){
	lock_file_queue *befree = NULL;
	server_response response;
	memset(&response, 0, sizeof response);
	response.code[0] = FILE_OPERATION_FAILED | FILE_NOT_EXISTS;
	response.code[1] = ENOENT;
	response.pathlen = strlen(server_storage.storage_table[index]->name) + 1;
	response.pathname = (char *) calloc(response.pathlen, sizeof(char));
	strcpy(response.pathname, server_storage.storage_table[index]->name);
	if(server_storage.storage_table[index]->lock_waiters != NULL){
		while (server_storage.storage_table[index]->lock_waiters != NULL){
			respond_to_client(server_storage.storage_table[index]->lock_waiters->com, response);
			sendback_client(server_storage.storage_table[index]->lock_waiters->com, false);
			befree = server_storage.storage_table[index]->lock_waiters;
			server_storage.storage_table[index]->lock_waiters = server_storage.storage_table[index]->lock_waiters->next;
			free(befree);
		}
	}
	free(response.pathname);
}

static int evict_victim(int index){
	unsigned long memory_freed = 0;
	start_write(index);
	memory_freed = server_storage.storage_table[index]->size;
	SAFELOCK(storage_access_mtx);
	server_storage.size -= server_storage.storage_table[index]->size;
	server_storage.file_count -= 1; 
	SAFEUNLOCK(storage_access_mtx);
	free(server_storage.storage_table[index]->data);
	server_storage.storage_table[index]->data = NULL;
	server_storage.storage_table[index]->deleted = true;
	empty_lock_queue(index);
	stop_write(index);
	return memory_freed;
}

static int select_victim(int caller, int files_to_delete, unsigned long memory_to_free) {
	victim* victims = (victim *) calloc(server_storage.file_limit, sizeof(victim));
	int counter = 0, j = 0;
	unsigned long memory_freed = 0;
	SAFELOCK(storage_access_mtx);
	server_storage.total_evictions += 1;
	for (size_t i = 0; i < 2*server_storage.file_limit; i++){
		if(server_storage.storage_table[i] != NULL && (i == -1 || i != caller)){
			start_read(i);
			if(!server_storage.storage_table[i]->deleted){
				victims[counter].index = i;
				victims[counter].create_time = server_storage.storage_table[i]->create_time;
				victims[counter].last_modified = server_storage.storage_table[i]->last_modified;
				victims[counter].use_stat = server_storage.storage_table[i]->use_stat;
				counter++;
			}
			stop_read(i);
		}
	}
	SAFEUNLOCK(storage_access_mtx);
	victims = (victim *) realloc(victims, counter);
	qsort(victims, counter, sizeof(victim), compare);
	if(files_to_delete){
		evict_victim(victims[0].index);
		return 0;
	}
	while(j < counter && memory_freed < memory_to_free){
		memory_freed = evict_victim(victims[j].index);
		j++;
	}
	return 0;
}
/**
 * Check whether there's enough space in memory or not
 * 
 * @param new_size size of the file to be inserted
 * @param old_size size of the file to be replaced. 0 if the new file does not overwrite anything
 * 
 * @returns 0 if the operation is successful, -1 if the file is too big or there are no locked files to remove
 *
 */
static int check_memory(unsigned long new_size, int caller){
	unsigned long max_capacity = 0, actual_capacity = 0;
	SAFELOCK(storage_access_mtx);
	max_capacity = server_storage.size_limit;
	actual_capacity = server_storage.size;
	if(new_size > max_capacity){
		SAFEUNLOCK(storage_access_mtx);
		return -1;
	}
	SAFEUNLOCK(storage_access_mtx);
	if(actual_capacity + new_size <= max_capacity) return 0;
	return select_victim(caller, 0, (new_size + actual_capacity) - max_capacity);
}



static int check_count(){
	int file_count = 0, file_limit = 0;
	file_limit = server_storage.file_limit;
	SAFELOCK(storage_access_mtx);
	file_count = server_storage.file_count;
	if(file_count + 1 < file_limit){
		SAFEUNLOCK(storage_access_mtx);
		return 0;
	}
	SAFEUNLOCK(storage_access_mtx);
	return select_victim(-1, 1, 0);
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
	server_storage.total_evictions = 0;
	return 0;
}

void print_storage(){
	SAFELOCK(storage_access_mtx);
	size_t table_size = 2*server_storage.file_limit;
	SAFEUNLOCK(storage_access_mtx);
	char create_time[100];
	char last_modified[100];
	for (size_t i = 0; i < table_size; i++){
		SAFELOCK(storage_access_mtx);
		if(server_storage.storage_table[i] != NULL){
			SAFEUNLOCK(storage_access_mtx);
			start_read(i);
			strftime(create_time, 99, "%d-%m-%Y %X", localtime(&server_storage.storage_table[i]->create_time));
			strftime(last_modified, 99, "%d-%m-%Y %X", localtime(&server_storage.storage_table[i]->last_modified));
			printf("----------\nName: %s\nSize: %lu\nUse_stat: %d\nCreated time: %s\nLast modified: %s\nLocker: %d\n----------\n\n", 
				server_storage.storage_table[i]->name, server_storage.storage_table[i]->size, server_storage.storage_table[i]->use_stat, create_time, last_modified, server_storage.storage_table[i]->whos_locking);
			stop_read(i);
			}
		SAFEUNLOCK(storage_access_mtx);
	}
}

void print_storage_info(){
	char memory[20];
	char files[20];
	SAFELOCK(storage_access_mtx);
	snprintf(memory, 20, "%lu/%lu", server_storage.size, server_storage.size_limit); 
	snprintf(files, 20, "%u/%u", server_storage.file_count, server_storage.file_limit); 
	SAFEUNLOCK(storage_access_mtx);
	printf(ANSI_COLOR_CYAN"»»» %-12s\t"ANSI_COLOR_YELLOW"%20s"ANSI_COLOR_CYAN" \n"
			"»»» %-12s\t"ANSI_COLOR_YELLOW"%20s"ANSI_COLOR_GREEN" \n" ANSI_COLOR_RESET, "Memory:",
			memory, "Files:", files);
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

	int file_index = search_file(filename);
	bool file_exists = false;
	bool create_file = false;
	bool lock_file = false;
	if(file_index != -1) file_exists = true;
	if(flags & O_CREATE) create_file = true;
	if(flags & O_LOCK) lock_file = true;

	if(file_exists && create_file){
		response->code[0] = FILE_OPERATION_FAILED | FILE_EXISTS;
		return -1;
	}
	
	if(!file_exists && !create_file){
		response->code[0] = FILE_OPERATION_FAILED;
		return -1;
	}

	if(!file_exists){
		if(check_count() == 0)
			init_file(client_id, filename, lock_file);
		else{
			response->code[0] = FILE_OPERATION_FAILED;
			response->code[1] = ENOMEM;
			return -1;
		}
	} 
	
	else if(file_exists){
		start_write(file_index);
		if(server_storage.storage_table[file_index]->whos_locking != client_id && server_storage.storage_table[file_index]->whos_locking != -1){
			stop_write(file_index);
			response->code[0] = FILE_OPERATION_FAILED | FILE_LOCKED_BY_OTHERS;
			return -1;
		}
		if(insert_client_file_list(&server_storage.storage_table[file_index]->clients_open, client_id) < 0){
			stop_write(file_index);
			response->code[0] = FILE_ALREADY_OPEN;
			return 0;
		}
 		if(lock_file){
			if(server_storage.storage_table[file_index]->whos_locking == -1) server_storage.storage_table[file_index]->whos_locking = client_id;
			else{
				remove_client_file_list(&server_storage.storage_table[file_index]->clients_open, client_id);
				stop_write(file_index);
				response->code[0] = FILE_OPERATION_FAILED | FILE_LOCKED_BY_OTHERS;
				response->code[1] = EBUSY;
				return -1;
			}
		}
		server_storage.storage_table[file_index]->use_stat += 1;
		stop_write(file_index);
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
	int file_index = search_file(filename);
	if(file_index == -1){
		response->code[0] = FILE_OPERATION_FAILED | FILE_NOT_EXISTS;
		response->code[1] = ENOENT;
		return -1; 
	}
	start_write(file_index);
	if(server_storage.storage_table[file_index]->whos_locking != client_id && server_storage.storage_table[file_index]->whos_locking != -1){
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
	int file_index = search_file(filename);
	if(file_index == -1){
		response->code[0] = FILE_OPERATION_FAILED | FILE_NOT_EXISTS;
		response->code[1] = ENOENT;
		return -1; 
	}

	start_write(file_index);
	SAFELOCK(storage_access_mtx);
	server_storage.size -= server_storage.storage_table[file_index]->size;
	server_storage.file_count -= 1;
	SAFEUNLOCK(storage_access_mtx);
	if(server_storage.storage_table[file_index]->whos_locking != client_id){
		stop_write(file_index);
		response->code[0] = FILE_OPERATION_FAILED | FILE_LOCKED_BY_OTHERS;
		response->code[1] = EACCES;
		return -1; 
	}
	server_storage.storage_table[file_index]->deleted = true;
	free(server_storage.storage_table[file_index]->data);
	server_storage.storage_table[file_index]->data = NULL;
	server_storage.storage_table[file_index]->size = 0;
	empty_lock_queue(file_index);
	stop_write(file_index);
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
		response->data = (unsigned char *) calloc(server_storage.storage_table[file_index]->size, sizeof(unsigned char));
		CHECKALLOC(response->data, "Errore allocazione memoria read_file");
		response->size = server_storage.storage_table[file_index]->size;
		memcpy(response->data, server_storage.storage_table[file_index]->data, response->size);
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
int read_n_file(int *last_index, int client_id, server_response* response){
	SAFELOCK(storage_access_mtx);
	while(*last_index < 2*server_storage.file_limit && server_storage.storage_table[*last_index] == NULL) *last_index += 1;
	SAFEUNLOCK(storage_access_mtx);
	while(*last_index < 2*server_storage.file_limit){
		start_read(*last_index);
		if(server_storage.storage_table[*last_index]->whos_locking == -1 || server_storage.storage_table[*last_index]->whos_locking == client_id){
			response->data = (unsigned char *) calloc(server_storage.storage_table[*last_index]->size, sizeof(unsigned char));
			CHECKALLOC(response->data, "Errore allocazione memoria read_file");
			response->size = server_storage.storage_table[*last_index]->size;
			memcpy(response->data, server_storage.storage_table[*last_index]->data, response->size);
			response->pathlen = strlen(basename(server_storage.storage_table[*last_index]->name))+1;
			response->pathname = (char *) calloc(response->pathlen, sizeof(char));
			strncpy(response->pathname, basename(server_storage.storage_table[*last_index]->name), response->pathlen-1);
			/* QUI HO FINITO DI LEGGERE ED ESCO */
			stop_read(*last_index);
			*last_index += 1;
			response->code[0] = FILE_OPERATION_SUCCESS;
			return 0;	
		}
		if(server_storage.storage_table[*last_index]->whos_locking != -1 && server_storage.storage_table[*last_index]->whos_locking != client_id){
			stop_read(*last_index);
			SAFELOCK(storage_access_mtx);
			while(*last_index < 2*server_storage.file_limit && server_storage.storage_table[*last_index] == NULL) *last_index += 1;
			SAFEUNLOCK(storage_access_mtx);
		}
	}
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
 * @param filename pathname of the file
 * @param client_id id of the client reading the file
 * @param response pointer to the server_response
 * 
 * @returns set the response and returns 0 if successful, -1 if something went wrong
 *
 */
int write_to_file(unsigned char *data, int length, char *filename, int client_id, server_response *response){
	int file_index = search_file(filename);
	// printf("FILE INDEX: %d\n", file_index);
	if(file_index == -1){
		response->code[1] = ENOENT;
		response->code[0] = FILE_OPERATION_FAILED | FILE_NOT_EXISTS;
		return -1;
	}
	/* QUI INIZIA LO SCRITTORE */
	start_write(file_index);

	/* QUI SI SCRIVE */
	if(server_storage.storage_table[file_index]->whos_locking == client_id){ // If a file is locked, it's already open
		if(check_memory(length, file_index) < 0){
			stop_write(file_index);
			response->code[1] = EFBIG;
			response->code[0] = FILE_OPERATION_FAILED;
			return -1;
		}
		server_storage.storage_table[file_index]->data = (unsigned char *) realloc(server_storage.storage_table[file_index]->data, length);
		CHECKALLOC(server_storage.storage_table[file_index], "Errore allocazione write_to_file");
		memcpy(server_storage.storage_table[file_index]->data, data, length);
		server_storage.storage_table[file_index]->size = length;
		server_storage.storage_table[file_index]->use_stat += 1;
		server_storage.storage_table[file_index]->last_modified = time(NULL);
		
		/* QUI HO FINITO DI SCRIVERE ED ESCO */
		stop_write(file_index);
		SAFELOCK(storage_access_mtx);
		server_storage.size += length;
		if(server_storage.size > server_storage.max_size_reached)  server_storage.max_size_reached = server_storage.size;
		pthread_cond_signal(&start_LFU_selector);
		SAFEUNLOCK(storage_access_mtx);
		response->code[0] = FILE_OPERATION_SUCCESS;
		
		return 0;
	}
	stop_write(file_index);
	response->code[1] = EACCES;
	response->code[0] = FILE_OPERATION_FAILED | FILE_NOT_LOCKED;
	return -1;
}

/**
 * Append new_data in the file identified by filename.
 * 
 * @param new_data data to be written
 * @param new_data_size length of new_data in bytes
 * @param filename pathname of the file
 * @param client_id id of the client reading the file
 * @param response pointer to the server_response
 * 
 * @returns set the response and returns 0 if successful, -1 if something went wrong
 *
 */
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
		if(check_memory(new_data_size + old_size, file_index) < 0){
			stop_write(file_index);
			response->code[1] = EFBIG;
			response->code[0] = FILE_OPERATION_FAILED;
			return -1;
		}
		
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
	max_len = 2*server_storage.file_limit;
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

static void init_file(int id, char *filename, bool locked){ // return 0, -1 on success or error
	int index = get_free_index(filename);
	SAFELOCK(storage_access_mtx);
	server_storage.file_count += 1;
	if(server_storage.storage_table[index] != NULL){
		if(server_storage.storage_table[index]->name){
			free(server_storage.storage_table[index]->name);
			server_storage.storage_table[index]->name = NULL;
		} 
		server_storage.storage_table[index]->name = (char *)calloc(strlen(filename) + 1, sizeof(char));
		CHECKALLOC(server_storage.storage_table[index]->name, "Errore inserimento nuovo file");
		strncpy(server_storage.storage_table[index]->name, filename, strlen(filename));
		server_storage.storage_table[index]->deleted = false;
		if(locked) server_storage.storage_table[index]->whos_locking = id;
		else server_storage.storage_table[index]->whos_locking = -1;
		insert_client_file_list(&server_storage.storage_table[index]->clients_open, id);
		server_storage.storage_table[index]->use_stat = 10;
		server_storage.storage_table[index]->readers = 0;
		server_storage.storage_table[index]->writers = 0;
		return;
	}
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
	server_storage.storage_table[index]->use_stat = 10;
	server_storage.storage_table[index]->readers = 0;
	server_storage.storage_table[index]->writers = 0;
	SAFEUNLOCK(storage_access_mtx);
}

void* use_stat_update(void *args){
	int table_size =  2*server_storage.file_limit;
	while(true){
		SAFELOCK(storage_access_mtx)
		pthread_cond_wait(&start_LFU_selector, &storage_access_mtx);
		SAFEUNLOCK(storage_access_mtx);
		for(int i = 0; i < table_size; i++){
			SAFELOCK(storage_access_mtx);
			if(server_storage.storage_table[i] == NULL){
				SAFEUNLOCK(storage_access_mtx);
				continue;
			}
			SAFEUNLOCK(storage_access_mtx);

			start_write(i);
			if(!server_storage.storage_table[i]->deleted && server_storage.storage_table[i]->use_stat != 0)
				server_storage.storage_table[i]->use_stat -= 1;
			if(!server_storage.storage_table[i]->deleted && server_storage.storage_table[i]->use_stat == 0 && (server_storage.storage_table[i]->last_modified - time(NULL)) > 120){
				server_storage.storage_table[i]->whos_locking = -1; // Automatic unlock if the file is not used for a while
				lock_next(server_storage.storage_table[i]->name, false);
			} 
			stop_write(i);
		}
	}
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
	server_storage.storage_table[file_index]->writers += 1;
	SAFEUNLOCK(server_storage.storage_table[file_index]->order_mutex); 
	SAFEUNLOCK(server_storage.storage_table[file_index]->access_mutex); 
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