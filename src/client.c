#ifndef COMMON_INCLUDES_H
#define COMMON_INCLUDES_H
#include "common_includes.h"
#endif
#include <getopt.h>
#ifndef CLIENT_H
#define CLIENT_H
#include "client.h"
#endif
#include "work.h"

int socket_fd;
client_conf config;


void signal_handler(int signum){
	
	if(signum == SIGHUP)
		puts(ANSI_COLOR_RED"Received SIGHUP"ANSI_COLOR_RESET);

	if(signum == SIGQUIT)
		puts(ANSI_COLOR_RED"Received SIGQUIT"ANSI_COLOR_RESET);
	if(signum == SIGINT)
		puts(ANSI_COLOR_RED"Received SIGINT"ANSI_COLOR_RESET);
	
	closeConnection(config.sockname);
	exit(EXIT_SUCCESS);
}

int main(int argc, char* argv[]){
	
	int opt;
	work_queue *job_queue[2];
	job_queue[0] = NULL;
	job_queue[1] = NULL;
	DIR* check_dir;
	char *save_dir = NULL;
	bool f = false, p = false;
	char buffer[100];
	struct timespec abstime = {
		.tv_nsec = 0,
		.tv_sec = SEC_TO_WAIT
	};
	memset(&config, 0, sizeof config);
	memset(buffer, 0, 100);
	
	
	struct sigaction sig; 
	memset(&sig, 0, sizeof(sig));
	sig.sa_handler = signal_handler;
	sigaction(SIGINT,&sig,NULL);
	sigaction(SIGHUP,&sig,NULL);
	sigaction(SIGQUIT,&sig,NULL);

	
	while ((opt = getopt(argc,argv, "hpf:r:W:w:R:d:t:l:u:c:D:xg")) != -1) {
		switch(opt) {
			case 'h': PRINT_HELP;
			case 'p': 
				if(!p){	
					p = true;
					config.verbose = true; 
				}
				else
					puts(ANSI_COLOR_RED"Output verboso già abilitato!"ANSI_COLOR_RESET);
				break;
			case 'f': 
				if(!f){	
					f = true;
					strncpy(config.sockname, optarg, AF_UNIX_MAX_PATH); 
				}
				else
					puts(ANSI_COLOR_RED"Socket File già specificato!"ANSI_COLOR_RESET);
				break;
			case 'r':
					enqueue_work(READ_FILES, optarg, &job_queue[0], &job_queue[1]);
					break;

			case 'W':
					enqueue_work(WRITE_FILES, optarg, &job_queue[0], &job_queue[1]);
				break;
			case 'w':
					enqueue_work(WRITE_DIR, optarg, &job_queue[0], &job_queue[1]);
				break;
			case 'R':
					enqueue_work(READ_N_FILES, optarg, &job_queue[0], &job_queue[1]);
				break;
			case 'D':
					save_dir = realpath(optarg, NULL);
					if(save_dir){
						if(!(check_dir = opendir(save_dir))){
							puts(ANSI_COLOR_RED"Cartella non valida, non sarà possibile salvare i file espulsi!"ANSI_COLOR_RESET);
							free(save_dir);
							save_dir = NULL;
						}
						else{
							closedir(check_dir);
							if(job_queue[1] && (job_queue[1]->command == WRITE_FILES || job_queue[1]->command == WRITE_DIR)){
								job_queue[1]->working_dir = calloc(strlen(save_dir) + 1, sizeof(char));
								strcpy(job_queue[1]->working_dir, save_dir);
								free(save_dir);
								save_dir = NULL;
							}
							else{
								puts(ANSI_COLOR_RED"Errore: -D deve essere preceduto da -w o -W!"ANSI_COLOR_RESET);
								free(save_dir);
								save_dir = NULL;
							}
						} 
					}
					else puts(ANSI_COLOR_RED"Cartella non valida, non sarà possibile salvare i file espulsi!"ANSI_COLOR_RESET);
				break;
			case 'd':
					save_dir = realpath(optarg, NULL);
					if(save_dir){
						if(!(check_dir = opendir(save_dir))){
							puts(ANSI_COLOR_RED"Cartella non valida, non sarà possibile salvare i file letti!"ANSI_COLOR_RESET);
							free(save_dir);
							save_dir = NULL;
						}
						else{
							closedir(check_dir);
							if(job_queue[1] && (job_queue[1]->command == READ_FILES || job_queue[1]->command == READ_N_FILES)){
								job_queue[1]->working_dir = calloc(strlen(save_dir) + 1, sizeof(char));
								strcpy(job_queue[1]->working_dir, save_dir);
								free(save_dir);
								save_dir = NULL;
							}
							else{
								puts(ANSI_COLOR_RED"Errore: -d deve essere preceduto da -r o -R!"ANSI_COLOR_RESET);
								free(save_dir);
								save_dir = NULL;
							}
						} 
					}
					else puts(ANSI_COLOR_RED"Cartella non valida, non sarà possibile salvare i file letti!"ANSI_COLOR_RESET);
				break;
			case 'x':
					if(job_queue[1] && job_queue[1]->command == WRITE_DIR)
						job_queue[1]->is_locked = false;
					else puts(ANSI_COLOR_RED"Errore: -x deve essere preceduto da -w!"ANSI_COLOR_RESET);

				break;
			case 't':
					errno = 0;
					config.interval = strtol(optarg, NULL, 10);
					if(errno != 0){
						perror("L'intervallo richiesto non è valido");
						config.interval = 0;
						errno = 0;
					}
				break;
			case 'l':
					enqueue_work(LOCK_FILES, optarg, &job_queue[0], &job_queue[1]);
				break;
			case 'u':
					enqueue_work(UNLOCK_FILES, optarg, &job_queue[0], &job_queue[1]);
				break;
			case 'c':
					enqueue_work(DELETE_FILES, optarg, &job_queue[0], &job_queue[1]);
				break;
			case ':': 
				printf("l'opzione '-%c' richiede un argomento\n", optopt);
				break;
			case '?':  
				printf("l'opzione '-%c' non e' gestita\n", optopt);
				break;
			default:;
		}
	}
	CHECKSCEXIT(openConnection(config.sockname, RETRY_AFTER, abstime), true, "Errore connessione");
	do_work(&job_queue[0], &job_queue[1]);
	CHECKERRNO(closeConnection(config.sockname) < 0, "Errore disconnessione");
	
	return 0;
}