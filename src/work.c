#include "work.h"

int handle_read_files(char **args){
	char *tmpstr = NULL;
	char *token = NULL;
	int fd = 0;
	unsigned char *buffer = NULL;
	size_t buff_size = 0;

	if(config.dirname) chdir(config.dirname);
	else puts(ANSI_COLOR_RED"Non è stata specificata la cartella di destinazione, i file non verranno salvati!"ANSI_COLOR_RESET);
	
	token = strtok_r(*args, DELIM, &tmpstr);
	while(token){
		CHECKERRNO(readFile(token, (void **)&buffer, &buff_size), "Errore lettura file");
		if(config.dirname){
			fd = open(basename(token), O_CREAT);
			CHECKSCEXIT(fd, true, "Non sono riuscito ad aprire il file");
			CHECKSCEXIT(write(fd, buffer, buff_size), true, "Errore scrittura file nella cartella");
			close(fd);
		}
		free(buffer);
		token = strtok_r(NULL, DELIM, &tmpstr);
	}
	free(*args);
	*args = NULL;
	return 0;
}

int handle_simple_request(char **args, unsigned char command){
	char *tmpstr = NULL;
	char *token = NULL;
	token = strtok_r(*args, DELIM, &tmpstr);
	char real_path[PATH_MAX];
	memset(real_path, 0, sizeof(char));
	while(token){
		realpath(token, real_path);
		if(command & WRITE_FILES){ 
			if(openFile(real_path, O_CREATE | O_LOCK) >= 0)
				CHECKERRNO(writeFile(real_path, NULL) < 0, "Errore scrittura file");
		}
		else if(command & LOCK_FILES){ 
			CHECKERRNO(lockFile(real_path) < 0, "Errore lock file");
		}
		else if(command & UNLOCK_FILES){ 
			CHECKERRNO(unlockFile(real_path) < 0, "Errore lock file");
		}
		else if(command & DELETE_FILES){ 
			if(openFile(real_path, O_CREATE | O_LOCK) >= 0)
				CHECKERRNO(removeFile(real_path), "Errore lock file");
		}
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
		if(command & WRITE_DIR) return NULL;
		else if(command & READ_FILES) handle_read_files(&args);
		else if(command & READ_N_FILES) return NULL;
		else handle_simple_request(&args, command);
		free(args);
		args = NULL;
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