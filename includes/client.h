#ifndef COMMON_INCLUDES_H
#define COMMON_INCLUDES_H
#include "common_includes.h"
#endif




typedef struct client_{
	bool verbose;
	// bool isHelp;
	time_t interval;
	char sockname[AF_UNIX_MAX_PATH];
	char dirname[UNIX_MAX_PATH];
} client_conf;




#define PRINT_HELP printf(ANSI_CLEAR_SCREEN"-h\t\tMostra questo messaggio\n\n" \
			"-p\t\tAbilita le stampe di ogni operazione sullo standard output\n\n" \
			"-f filename\tSpecifica il nome del socket a cui connettersi\n\n" \
			"-w dirname[n=0]\tInvia al server n file della cartella 'dirname'.\n               \tSe n = 0 o non specificato, si invierà il maggior\n               \tnumero di file che il server riesce a gestire.\n\n" \
			"-D dirname\tNon ho capito per ora\n\n" \
			"-R [n = 0]\tQuesta opzione permettere di leggere n file qualsiasi memorizzati sul server.\n          \tSe n non è specificato, si leggeranno tutti i file presenti sul server.\n\n" \
			"-d dirname\tSpecifica dove salvare i file letti da server.\n"ANSI_COLOR_RED"          \tSe non viene specificata la cartella, i file non verranno salvati!\n\n" ANSI_COLOR_RESET \
			"-t time\t\tTempo in millisecondi che intercorre fra l'invio di due richieste successive al server.\n       \t\tSe non specificato (-t 0), non si ha delay fra le richieste\n\n" \
			"-W file1[,fileN]\tFile da inviare al server separati da una virgola\n\n" \
			"-r file1[,file2]\tFile da leggere dal server separati da una virgola\n\n" \
			"-l file1[,file2]\tFile su cui acquisire la mutex\n\n" \
			"-u file1[,file2]\tFile su cui rilasciare la mutex\n\n" \
			"-c file1[,file2]\tFile da rimuovere dal server" \
			"\n"); \
	exit(EXIT_SUCCESS)


