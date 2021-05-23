#ifndef COMMON_INCLUDES_H
#define COMMON_INCLUDES_H
#include "common_includes.h"
#endif
#include "fssApi.h"
#include <getopt.h>
bool verbose = false; // tipo di op, file su cui si opera, exito e byte letti/scritti


int socket_fd;

void print_help(){
	printf("-h\t\tMostra questo messaggio\n\n"
			"-p\t\tAbilita le stampe di ogni operazione sullo standard output\n\n"
			"-f filename\tSpecifica il nome del socket a cui connettersi\n\n"
			"-w dirname[n=0]\tInvia al server n file della cartella 'dirname'.\n               \tSe n = 0 o non specificato, si invierà il maggior\n               \tnumero di file che il server riesce a gestire.\n\n"
			"-D dirname\tNon ho capito per ora\n\n"
			"-R [n = 0]\tQuesta opzione permettere di leggere n file qualsiasi memorizzati sul server.\n          \tSe n non è specificato, si leggeranno tutti i file presenti sul server.\n\n"
			"-d dirname\tSpecifica dove salvare i file letti da server.\n"ANSI_COLOR_RED"          \tSe non viene specificata la cartella, i file non verranno salvati!\n\n" ANSI_COLOR_RESET
			"-t time\t\tTempo in millisecondi che intercorre fra l'invio di due richieste successive al server.\n       \t\tSe non specificato (-t 0), non si ha delay fra le richieste\n\n"
			"-W file1[,fileN]\tFile da inviare al server separati da una virgola\n\n"
			"-r file1[,file2]\tFile da leggere dal server separati da una virgola\n\n"
			"-l file1[,file2]\tFile su cui acquisire la mutex\n\n"
			"-u file1[,file2]\tFile su cui rilasciare la mutex\n\n"
			"-c file1[,file2]\tFile da rimuovere dal server"
			"\n");
	exit(EXIT_SUCCESS);

}

// void signal_handler(int signum){
	
// 	if(signum == SIGHUP)
// 		puts(ANSI_COLOR_RED"Received SIGHUP"ANSI_COLOR_RESET);

// 	if(signum == SIGQUIT)
// 		puts(ANSI_COLOR_RED"Received SIGQUIT"ANSI_COLOR_RESET);
// 	if(signum == SIGINT)
// 		puts(ANSI_COLOR_RED"Received SIGINT"ANSI_COLOR_RESET);
	
// 	write(socket_fd, QUIT, strlen(QUIT));
// 	close(socket_fd);
// 	exit(EXIT_SUCCESS);
		
// }



int main(int argc, char* argv[]){
	printf(ANSI_CLEAR_SCREEN);
	int opt;
	bool f = false, p = false;
	char buffer[100];
	char sockname[UNIX_MAX_PATH];
	unsigned char *databuffer = NULL;
	size_t data_size = 0;
	struct timespec abstime = {
		.tv_nsec = 0,
		.tv_sec = 3
	};
	memset(buffer, 0, 100);
	memset(sockname, 0, UNIX_MAX_PATH);

	// struct sigaction sig; 
	// memset(&sig, 0, sizeof(sig));
	// sig.sa_handler = signal_handler;
	// sigaction(SIGINT,&sig,NULL);
	// sigaction(SIGHUP,&sig,NULL);
	// sigaction(SIGQUIT,&sig,NULL);

	
	while ((opt = getopt(argc,argv, "hpf:r:W:")) != -1) {
		switch(opt) {
			case 'h': print_help(); break;
			case 'p': 
				if(!p){	
					p = true;
					verbose = true; 
					printf(ANSI_COLOR_GREEN"-> Abilitato output verboso <-\n"ANSI_COLOR_RESET); 
				}
				else
					puts(ANSI_COLOR_RED"Output verboso già abilitato!"ANSI_COLOR_RESET);
				break;
			case 'f': 
				if(!f){	
					f = true;
					strncpy(sockname, optarg, UNIX_MAX_PATH); 
				}
				else
					puts(ANSI_COLOR_RED"Socket File già specificato!"ANSI_COLOR_RESET);
				break;
			case 'r':
					CHECKSCEXIT(openConnection(sockname, 500, abstime), true, "Errore di connesione");
					readFile(optarg, &databuffer, &data_size);
					for (size_t i = 0; i < data_size; i++){
						printf("%c", databuffer[i]);
					}
					
				break;

			case 'W':
					CHECKSCEXIT(openConnection(sockname, 500, abstime), true, "Errore di connesione");
					openFile(optarg, O_CREATE);
					puts("aperto");
					writeFile(optarg, NULL);
					puts("scritto");
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
	


	CHECKSCEXIT(openConnection(sockname, 500, abstime), true, "Errore di connesione");
	puts("Prima della open");
	openFile("README.md", O_CREATE | O_LOCK);
	puts("aperto");
	writeFile("README.md", NULL);
	puts("scritto");

	memset(buffer, 0, sizeof(buffer));
	while(true){
		fgets(buffer, 98, stdin);
		buffer[strcspn(buffer, "\n")] = 0;
		while(strlen(buffer) == 0){
			fgets(buffer, 98, stdin);
			buffer[strcspn(buffer, "\n")] = 0;
		}
		if(strcmp(buffer, "quit") == 0){
			write(socket_fd, buffer, strlen(buffer));
			break;
		}
		write(socket_fd, buffer, strlen(buffer));
		memset(buffer, 0, 100);
		
		read(socket_fd, buffer, sizeof(buffer));
		memset(buffer, 0, 100);
	}
	CHECKSCEXIT(closeConnection(sockname), true, "Errore di disconnesione");
	// 	}
	// }
	// else
		// puts(ANSI_COLOR_RED"Connection refused\n"ANSI_COLOR_RESET);
	
	return 0;
}