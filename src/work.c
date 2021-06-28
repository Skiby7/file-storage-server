#include "work.h"

int handle_read_files(char **args){
	char *tmpstr = NULL;
	char *token = NULL;
	int fd = 0;
	unsigned char *buffer = NULL;
	size_t buff_size = 0;
	char* real_path = 0;
	if(strlen(config.dirname)) chdir(config.dirname);
	else puts(ANSI_COLOR_RED"Non è stata specificata la cartella di destinazione, i file non verranno salvati!"ANSI_COLOR_RESET);
	
	token = strtok_r(*args, DELIM, &tmpstr);
	while(token){
		real_path = realpath(token, NULL);
		if(openFile(real_path, 0) >= 0)
			readFile(real_path, (void **)&buffer, &buff_size);
		if(config.verbose) printf("Read %lu bytes from %s\n", buff_size, token);
		if(strlen(config.dirname)){
			fd = open(basename(real_path), O_CREAT | O_RDWR, 0777);
			CHECKSCEXIT(fd, true, "Non sono riuscito ad aprire il file");
			CHECKSCEXIT(write(fd, buffer, buff_size), true, "Errore scrittura file nella cartella");
			close(fd);
		}
		free(buffer);
		free(real_path);
		real_path = NULL;
		token = strtok_r(NULL, DELIM, &tmpstr);
	}
	free(*args);
	*args = NULL;
	return 0;
}

int handle_simple_request(char **args, unsigned char command){
	char *tmpstr = NULL;
	char *token = NULL;
	char *real_path = NULL;
	struct stat st;
	int write_size = 0;
	token = strtok_r(*args, DELIM, &tmpstr);
	
	while(token){
		real_path = realpath(token, NULL);
		if(command & WRITE_FILES){ 
			if(openFile(real_path, O_CREATE | O_LOCK) >= 0){
				CHECKERRNO(writeFile(real_path, NULL) < 0, "Errore scrittura file");
				stat(real_path, &st);
				if(config.verbose) printf("Written %lu bytes (file: %s)\n", st.st_size, token);
			}
		}
		else if(command & LOCK_FILES){ 
			CHECKERRNO(lockFile(real_path) < 0, "Errore lock file");
			if(config.verbose) printf("Locked %s\n", token);

		}
		else if(command & UNLOCK_FILES){ 
			CHECKERRNO(unlockFile(real_path) < 0, "Errore unlock file");
			if(config.verbose) printf("Unlocked %s\n", token);

		}
		else if(command & DELETE_FILES){ 
			CHECKERRNO(removeFile(real_path), "Errore delete file");
			stat(real_path, &st);
			if(config.verbose) printf("Deleted %s, %lu bytes freed\n", token, st.st_size);
		}
		free(real_path);
		real_path = NULL;
		token = strtok_r(NULL, DELIM, &tmpstr);
	}
	free(*args);
	*args = NULL;
	return 0;
}

void do_work(work_queue **head, work_queue **tail){
	unsigned char command = 0;
	char* args = NULL;
	while((*tail)){
		dequeue_work(&command, &args, head, tail);
		if(command & WRITE_DIR) return;
		else if(command & READ_FILES) handle_read_files(&args);
		else if(command & READ_N_FILES) return;
		else handle_simple_request(&args, command);
		free(args);
		args = NULL;
		usleep(config.interval * 1000);
	}
}

void enqueue_work(unsigned char command, char *args, work_queue **head, work_queue **tail){
	work_queue* new = (work_queue*) malloc(sizeof(work_queue));
	CHECKALLOC(new, "Errore inserimento nella lista pronti");
	new->command = command;
	new->args = (char *) calloc((strlen(args)+1), sizeof(char));
	strncpy(new->args, args, strlen(args));
	new->next = (*head);
	new->prev = NULL;
	if((*tail) == NULL)
		(*tail) = new;
	if((*head) != NULL)
		(*head)->prev = new;
	(*head) = new;	
} 

int dequeue_work(unsigned char* command, char **args, work_queue **head, work_queue **tail){
	work_queue *befree = NULL;
	if((*tail) == NULL)
		return -1;

	*command = (*tail)->command;
	*args = (char *) calloc((strlen((*tail)->args))+1, sizeof(char));
	strncpy(*args, (*tail)->args, (strlen((*tail)->args)));
	free((*tail)->args);
	befree = (*tail);
	if((*tail)->prev != NULL)
		(*tail)->prev->next = NULL;
	
	if(((*tail) = (*tail)->prev) == NULL)
		(*head) = NULL;
	free(befree);
	return 0;
} 