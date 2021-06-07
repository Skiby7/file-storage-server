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
bool verbose = false; // tipo di op, file su cui si opera, exito e byte letti/scritti
void enqueue_work(unsigned char command, char *args, work_queue **head, work_queue **tail);
int pop_work(unsigned char* command, char **args, work_queue **head, work_queue **tail);

int socket_fd;
char sockname[AF_UNIX_MAX_PATH];
client_conf config;


void signal_handler(int signum){
	
	if(signum == SIGHUP)
		puts(ANSI_COLOR_RED"Received SIGHUP"ANSI_COLOR_RESET);

	if(signum == SIGQUIT)
		puts(ANSI_COLOR_RED"Received SIGQUIT"ANSI_COLOR_RESET);
	if(signum == SIGINT)
		puts(ANSI_COLOR_RED"Received SIGINT"ANSI_COLOR_RESET);
	
	// closeConnection(sockname);
	exit(EXIT_SUCCESS);
}





int main(int argc, char* argv[]){
	printf(ANSI_CLEAR_SCREEN);
	int opt;
	work_queue *job_queue[2];
	job_queue[0] = NULL;
	job_queue[1] = NULL;

	bool f = false, p = false;
	char buffer[100];
	unsigned char* databuffer = NULL;
	size_t data_size = 0;
	char pathname_tmp[PATH_MAX];
	struct timespec abstime = {
		.tv_nsec = 0,
		.tv_sec = 3
	};
	memset(&config, 0, sizeof config);
	memset(buffer, 0, 100);
	memset(sockname, 0, AF_UNIX_MAX_PATH);
	
	struct sigaction sig; 
	memset(&sig, 0, sizeof(sig));
	sig.sa_handler = signal_handler;
	sigaction(SIGINT,&sig,NULL);
	sigaction(SIGHUP,&sig,NULL);
	sigaction(SIGQUIT,&sig,NULL);

	
	while ((opt = getopt(argc,argv, "hpf:r:W:o:")) != -1) {
		switch(opt) {
			case 'h': PRINT_HELP; break;
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
			case 'o':
					puts("prima open");
					realpath(optarg, pathname_tmp);
					// CHECKERRNO(openFile(pathname_tmp, 0), "Errore open");
					puts("dopo open");
					break;
			case 'r':
					enqueue_work(READ_FILES, optarg, &job_queue[0], &job_queue[1]);
					break;

			case 'W':
					enqueue_work(WRITE_FILES, optarg, &job_queue[0], &job_queue[1]);
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
	do_work(&job_queue[0], &job_queue[1]);
	// CHECKERRNO(openConnection(config.sockname, 500, abstime), "Errore connessione");
	
	
	
	
	return 0;
}





