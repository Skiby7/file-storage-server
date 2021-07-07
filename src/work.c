#include "work.h"

int handle_read_files(char *args, char *dirname){
	char *tmpstr = NULL;
	char *token = NULL;
	int fd = 0;
	unsigned char *buffer = NULL;
	size_t buff_size = 0;
	char* real_path = 0;
	int retval = 0;
	char current_dir[PATH_MAX];
	getcwd(current_dir, sizeof current_dir);
	if(!dirname) puts(ANSI_COLOR_RED"Non è stata specificata la cartella di destinazione, i file non verranno salvati!"ANSI_COLOR_RESET);
	
	
	token = strtok_r(args, DELIM, &tmpstr);
	while(token){
		real_path = realpath(token, NULL);
		retval = openFile(real_path, 0);
		CHECKERRNO(retval < 0, "Errore openFile!");
		if(retval>= 0){
			retval = readFile(real_path, (void **)&buffer, &buff_size);
			CHECKERRNO(retval < 0, "Errore readFile!");
			if(config.verbose && retval >= 0) printf("Letti %lu bytes da %s\n", buff_size, token);
		}
		
		if(dirname){
			chdir(dirname);
			fd = open(basename(real_path), O_CREAT | O_RDWR, 0777);
			CHECKSCEXIT(fd, true, "Non sono riuscito ad aprire il file");
			CHECKSCEXIT(write(fd, buffer, buff_size), true, "Errore scrittura file nella cartella");
			close(fd);
			chdir(current_dir);
		}
		free(buffer);
		free(real_path);
		real_path = NULL;
		token = strtok_r(NULL, DELIM, &tmpstr);
	}
	return 0;
}

int handle_simple_request(char *args, unsigned char command){
	char *tmpstr = NULL;
	char *token = NULL;
	char *real_path = NULL;
	int open_file_val = 0, file = 0;
	size_t size = 0;
	struct stat st;
	unsigned char *buffer = NULL;
	token = strtok_r(args, DELIM, &tmpstr);
	
	while(token){
		real_path = realpath(token, NULL);
		errno = 0;
		if(command & WRITE_FILES){ 
			open_file_val = openFile(real_path, O_CREATE | O_LOCK);
			if(open_file_val >= 0){
				CHECKERRNO(writeFile(real_path, NULL) < 0, "Errore scrittura file");
				CHECKERRNO(closeFile(real_path) < 0, "Errore chiusura file");
				stat(real_path, &st);
				if(config.verbose) printf("Scritti %lu bytes (file: %s)\n", st.st_size, token);
			}
			else if(open_file_val <= 0 && errno == EEXIST){
				errno = 0;
				if(openFile(real_path, 0) < 0){
					perror("Errore apertura file!");
					continue;
				}
				if((file = open(real_path, O_RDONLY)) == -1){
					perror("Errore durante l'apertura del file");
					continue;
				}
				if(fstat(file, &st) < 0){
					perror("Errore fstat");
					continue;
				}
				size = st.st_size;
				buffer =  (unsigned char *) calloc(size, sizeof(unsigned char));
				read(file, buffer, size);
				CHECKERRNO(appendToFile(real_path, buffer, size, NULL) < 0, "Errore scrittura file");
				CHECKERRNO(closeFile(real_path) < 0, "Errore chiusura file");
				if(config.verbose) printf("Appended %lu bytes (file: %s)\n", st.st_size, token);
				free(buffer);
				buffer = NULL;
			}
			else{
				perror("Errore apertura file!");
				continue;
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
			if(config.verbose) printf("Rimosso %s, liberati %lu bytes\n", token, st.st_size);
		}
		free(real_path);
		real_path = NULL;
		token = strtok_r(NULL, DELIM, &tmpstr);
	}
	return 0;
}

int recursive_visit(char *start_dir, int files_to_write, bool locked){
	DIR* target_dir = NULL;
	struct stat dirent_info, st;
    struct dirent* current_file;
	char *real_path = NULL;
	int files_written = 0, rec_visit = 0, open_file_val = 0, file = 0;
	size_t size = 0;
	unsigned char *buffer = NULL;

	if((target_dir = opendir(start_dir)) == NULL){
		return -1;
	}
	errno = 0;
	while((current_file = readdir(target_dir)) && (!files_to_write || files_written < files_to_write)){
		if(errno && !current_file) return -1; 
		if(strcmp(current_file->d_name, ".") == 0 || strcmp(current_file->d_name, "..") == 0) continue;
		real_path = realpath(current_file->d_name, NULL);
	 	if (stat(real_path, &dirent_info) == -1) {
            perror("Errore recupero informazioni del file!");
			continue;
        }
		if(S_ISDIR(dirent_info.st_mode)){
			rec_visit = recursive_visit(start_dir, files_to_write - files_written, locked);
			if(rec_visit < 0) return -1;
			files_written += rec_visit;
		}
		else{
			errno = 0;
			open_file_val = openFile(real_path, O_CREATE | O_LOCK);
			if(open_file_val >= 0){
				CHECKERRNO(writeFile(real_path, NULL) < 0, "Errore scrittura file");
				if(!locked) CHECKERRNO(unlockFile(real_path), "Errore unlock file");
				CHECKERRNO(closeFile(real_path) < 0, "Errore chiusura file");
				stat(real_path, &st);
				if(config.verbose) printf("Scritti %lu bytes (file: %s)\n", st.st_size, real_path);
			}
			else if(open_file_val <= 0 && errno == EEXIST){
				errno = 0;
				if(openFile(real_path, 0) < 0){
					perror("Errore apertura file!");
					continue;
				}
				if((file = open(real_path, O_RDONLY)) == -1){
					perror("Errore durante l'apertura del file");
					continue;
				}
				if(fstat(file, &st) < 0){
					perror("Errore fstat");
					continue;
				}
				size = st.st_size;
				buffer =  (unsigned char *) calloc(size, sizeof(unsigned char));
				read(file, buffer, size);
				CHECKERRNO(appendToFile(real_path, buffer, size, NULL) < 0, "Errore scrittura file");
				CHECKERRNO(closeFile(real_path) < 0, "Errore chiusura file");
				if(config.verbose) printf("Appended %lu bytes (file: %s)\n", st.st_size, real_path);
				free(buffer);
				buffer = NULL;
			}
			else{
				perror("Errore apertura file!");
				continue;
			}
			files_written++;
			free(real_path);
		}
	}
	if (closedir(target_dir) == -1) {
		perror("Errore chiusura cartella!");
		return -1;
	}
	return files_written;
}

int write_dir(char *args, bool is_locked){
	int N = 0, files_written = 0;;
	char *dirname = NULL;
	char *n = NULL;
	char *tmpstr = NULL;
	char *real_path = NULL;
	dirname = strtok_r(args, DELIM, &tmpstr);
	real_path = realpath(dirname, NULL);
	if(!real_path){
		puts(ANSI_COLOR_RED"Cartella non valida!"ANSI_COLOR_RESET);
		return -1;
	}
	if((n = strtok_r(NULL, DELIM, &tmpstr))){
		errno = 0;
		N = strtol(n, NULL, 10);
		if(errno){
			perror("Errore: n non è valido! Default n = 0");
			N = 0;
		}
	}
	files_written = recursive_visit(real_path, N, is_locked);
	if(files_written < 0) return -1;
	if(config.verbose) printf("Scritti %d files!\n", files_written);
	return 0;
}

void do_work(work_queue **head, work_queue **tail){
	unsigned char command = 0;
	char* args = NULL;
	char* dirname = NULL;
	int N = 0, files_read = 0;
	bool is_locked = true;
	while((*tail)){
		dequeue_work(&command, &args, &dirname, &is_locked, head, tail);
		if(command & WRITE_DIR) write_dir(args, is_locked);
		else if(command & READ_FILES) handle_read_files(args, dirname);
		else if(command & READ_N_FILES){
			errno = 0;
			N = strtol(args, NULL, 10);
			if(errno != 0){
				perror("Il numero inserito non è valido!");
				errno = 0;
				continue;
			}
			files_read = readNFile(N, dirname);
			printf("Letti %d files\n", files_read);
		}
		else handle_simple_request(args, command);
		free(args);
		args = NULL;
		if(dirname){
			free(dirname);
			dirname = NULL;
		}
		usleep(config.interval * 1000);
	}
}

void enqueue_work(unsigned char command, char *args, work_queue **head, work_queue **tail){
	work_queue* new = (work_queue*) malloc(sizeof(work_queue));
	CHECKALLOC(new, "Errore inserimento nella lista pronti");
	new->command = command;
	new->args = (char *) calloc((strlen(args)+1), sizeof(char));
	strncpy(new->args, args, strlen(args));
	new->is_locked = true;
	new->next = (*head);
	new->prev = NULL;
	if((*tail) == NULL)
		(*tail) = new;
	if((*head) != NULL)
		(*head)->prev = new;
	(*head) = new;	
} 

int dequeue_work(unsigned char* command, char **args, char **dirname, bool *is_locked, work_queue **head, work_queue **tail){
	work_queue *befree = NULL;
	if((*tail) == NULL)
		return -1;

	*command = (*tail)->command;
	*args = (char *) calloc((strlen((*tail)->args))+1, sizeof(char));
	*is_locked = (*tail)->is_locked;
	strncpy(*args, (*tail)->args, (strlen((*tail)->args)));
	if((*tail)->working_dir){
		*dirname = (char *)calloc((strlen((*tail)->working_dir)) + 1, sizeof(char));
		strncpy(*dirname, (*tail)->working_dir, strlen((*tail)->working_dir));
		free((*tail)->working_dir);
	}
	free((*tail)->args);
	befree = (*tail);
	if((*tail)->prev != NULL)
		(*tail)->prev->next = NULL;
	
	if(((*tail) = (*tail)->prev) == NULL)
		(*head) = NULL;
	free(befree);
	return 0;
} 