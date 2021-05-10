#ifndef COMMON_INCLUDES_H
#define COMMON_INCLUDES_H
#include "common_includes.h"
#endif
#include <getopt.h>
bool verbose = false; // tipo di op, file su cui si opera, exito e byte letti/scritti

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

int main(int argc, char* argv[]){
	printf(ANSI_CLEAR_SCREEN);
	int opt, socket_fd;
	bool f = false, p = false;
	char buffer[100];
	struct sockaddr_un sockaddress;
	memset(buffer, 0, 100);
	
	while ((opt = getopt(argc,argv, "hpf:")) != -1) {
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
					strncpy(sockaddress.sun_path, optarg, UNIX_MAX_PATH); 
					sockaddress.sun_family = AF_UNIX;
				}
				else
					puts(ANSI_COLOR_RED"File già specificato!"ANSI_COLOR_RESET);
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
	
	socket_fd = socket(AF_UNIX, SOCK_STREAM, 0);

	while(connect(socket_fd,(struct sockaddr*) &sockaddress,sizeof(sockaddress)) == -1);
	read(socket_fd, buffer, sizeof(buffer));
	if(strcmp(buffer, "accepted") == 0){
		memset(buffer, 0, sizeof(buffer));
		puts(ANSI_COLOR_CYAN"Connected\n"ANSI_COLOR_RESET);
		strcpy(buffer, "Client is ready!");
		write(socket_fd, buffer, strlen(buffer));
		memset(buffer, 0, sizeof(buffer));
		while(true){
			fgets(buffer, 98, stdin);
			buffer[strcspn(buffer, "\n")] = 0;
			if(strncmp(buffer, "quit", strlen(buffer)) == 0){
				write(socket_fd, buffer, strlen(buffer));
				break;
			}
			write(socket_fd, buffer, strlen(buffer));
			memset(buffer, 0, 100);
			read(socket_fd, buffer, 99);
			puts(buffer);
			memset(buffer, 0, 100);

		}
	}
	else
		puts(ANSI_COLOR_RED"Connection refused\n"ANSI_COLOR_RESET);
	close(socket_fd);
	return 0;
}