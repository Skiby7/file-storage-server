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
	printf(ANSI_CLEAR_SCREEN);
	int opt;
	work_queue *job_queue[2];
	job_queue[0] = NULL;
	job_queue[1] = NULL;
	DIR* check_dir;
	bool f = false, p = false;
	char buffer[100];

	char pathname_tmp[PATH_MAX];
	struct timespec abstime = {
		.tv_nsec = 0,
		.tv_sec = 3
	};
	memset(&config, 0, sizeof config);
	memset(buffer, 0, 100);
	
	struct sigaction sig; 
	memset(&sig, 0, sizeof(sig));
	sig.sa_handler = signal_handler;
	sigaction(SIGINT,&sig,NULL);
	sigaction(SIGHUP,&sig,NULL);
	sigaction(SIGQUIT,&sig,NULL);

	
	while ((opt = getopt(argc,argv, "hpf:r:W:w:R:d:t:l:u:c:")) != -1) {
		switch(opt) {
			case 'h': PRINT_HELP;
			case 'p': 
				if(!p){	
					p = true;
					config.verbose = true; 
					printf(ANSI_COLOR_GREEN"-> Abilitato output verboso <-\n"ANSI_COLOR_RESET); 
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
			case 'd':
					if((check_dir = opendir(optarg))){
						strncpy(config.dirname, realpath(optarg, NULL), UNIX_MAX_PATH);
						close(check_dir);
					}
					else puts(ANSI_COLOR_RED"Cartella non valida, non sarà possibile salvare i file letti!"ANSI_COLOR_RESET);
				break;
			case 't':
					errno = 0;
					config.interval = strtol(optarg, NULL, 10);
					if(errno != 0){
						perror("L'intervallo richiesto non è valido");
						config.interval = 0;
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
			case ':': {
			printf("l'opzione '-%c' richiede un argomento\n", optopt);
			} break;
			case '?': {  // restituito se getopt trova una opzione non riconosciuta
			printf("l'opzione '-%c' non e' gestita\n", optopt);
			} break;
			default:;
		}
	}
	unsigned char* databuffer = NULL;
	size_t datasize = 512;
	databuffer = calloc(512, 1);
	for (size_t i = 0; i < 512; i++)
	{
		databuffer[i] = rand()%127;
	}
	
	CHECKERRNO(openConnection(config.sockname, 500, abstime) < 0, "Errore connessione");
	// puts("connesso");
	// do_work(&job_queue[0], &job_queue[1]);
	openFile("README.md", O_CREATE | O_LOCK);
	// openFile("Makefile", O_CREATE | O_LOCK);
	// openFile("input", O_CREATE | O_LOCK);
	appendToFile("README.md", databuffer, 512, NULL);
	readFile("README.md", (void**)&databuffer, &datasize);
	// readFile("Makefile", (void**)&databuffer, &datasize);
	// readFile("input", (void**)&databuffer, &datasize);
	CHECKERRNO(closeConnection(config.sockname) < 0, "Errore disconnessione");
	
	free(databuffer);
	
	
	
	return 0;
}





