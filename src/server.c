#include "server.h"
#include "parser.h"
#include "file.h"
#include "client_queue.h"
#define _GNU_SOURCE 
#define DEFAULTFDS 10



config configuration; // Server config
volatile sig_atomic_t can_accept = true;
volatile sig_atomic_t abort_connections = false;
pthread_mutex_t ready_queue_mtx = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t pollfd_access = PTHREAD_COND_INITIALIZER;
pthread_cond_t client_is_ready = PTHREAD_COND_INITIALIZER;
bool *free_threads;
ready_clients *ready_queue[2];
int m_w_pipe[2]; // 1 lettura, 0 scrittura
extern void* worker(void* args);
pthread_mutex_t free_threads_mtx = PTHREAD_MUTEX_INITIALIZER;


/* TODO:
*	- Coda socket ready su cui fare la select e da cui prendere lavori
*	- Coda socket finished da reinserire in ascolto
*	- Rimozione da ready -> invio al worker -> reinserimento su finished/listen
*	- Comunicazione master-slave con pipe
*	- Testare riallocazione di com_fd
*	- Segnale di arrivo client
*	- Wait su cond di empty client queue da parte dei thread spawnati
*
*/

int main(int argc, char* argv[]){ // REMEMBER FFLUSH FOR THREAD PRINTF
	
	int socket_fd = 0, com = 0,  read_bytes = 0, tmp = 0, poll_val = 0; // i = 0, ready_com = 0
	
	char buff[20]; // Buffer per inviare messaggi sullo stato dell'accettazione al client
	char SOCKETADDR[UNIX_MAX_PATH]; // Indirizzo del socket
	struct pollfd *com_fd =  (struct pollfd *) malloc(DEFAULTFDS*sizeof(struct pollfd));
	nfds_t com_count = 0;
	nfds_t com_size = 0;
	ready_queue[0] = NULL;
	ready_queue[1] = NULL;
	CHECKALLOC(com_fd, "pollfd");
	
	pthread_t *workers;
	struct sockaddr_un sockaddress; // Socket init
	// unsigned int seed = time(NULL);
	
	init(SOCKETADDR); // Configuration struct is now initialized
	PRINT_WELCOME;
	printconf(SOCKETADDR);
	// Signal handler
	// struct sigaction sig; 
	// memset(&sig, 0, sizeof(sig));
	// sig.sa_handler=signal_handler;
	// sigaction(SIGINT,&sig,NULL);
	// sigaction(SIGHUP,&sig,NULL);
	// sigaction(SIGQUIT,&sig,NULL);
	// END signal handler
	puts(">> Signal_handler installato...");

	
	puts(">> Inizializzo i workers...");
	workers = (pthread_t *) malloc(configuration.workers*sizeof(pthread_t)); // Pool di workers
	CHECKALLOC(workers, "workers array");
	memset(workers, 0, configuration.workers*sizeof(pthread_t));
	puts(">> Workers array inizializzato...");

	free_threads = (bool *) malloc(configuration.workers*sizeof(bool));
	memset(free_threads, true, configuration.workers*sizeof(bool));
	CHECKALLOC(free_threads, "workers array");

	memset(com_fd, -1, sizeof(struct pollfd));
	memset(buff, 0, 10);
	CHECKEXIT((pipe(m_w_pipe) == -1), true, "Impossibile inizializzare la pipe");
	puts(">> Pipe inzializzata...");

	strncpy(sockaddress.sun_path, SOCKETADDR, UNIX_MAX_PATH);
	sockaddress.sun_family = AF_UNIX;
	socket_fd = socket(AF_UNIX, SOCK_STREAM, 0);
	unlink(SOCKETADDR);
	CHECKSCEXIT(bind(socket_fd, (struct sockaddr *) &sockaddress, sizeof(sockaddress)), true, "Non sono riuscito a fare la bind");
	CHECKSCEXIT(listen(socket_fd, 10), true, "Impossibile effettuare la listen");
	puts(">> Ho inizializzato il socket ed ho eseguito la bind e la listen...");

	com_fd[0].fd = socket_fd;
	com_fd[0].events = POLLIN;
	com_fd[1].fd = m_w_pipe[0];
	com_fd[1].events = POLLIN;
	com_count = 2;
	com_size = com_count;
	puts(">> Polling struct inizializzata con il socket_fd su i = 0 e l'endpoint della pipe su i = 1...");
	printf(ANSI_COLOR_MAGENTA">> Partito Thread");
	for (int i = 0; i < configuration.workers; i++){
		printf(", %d", i);
		pthread_create(&workers[i], NULL, &worker, &i);
		pthread_detach(workers[i]);
	}
	puts(ANSI_COLOR_RESET);
	while(true){

		if(can_accept){	
			puts(ANSI_COLOR_GREEN"Polling..."ANSI_COLOR_RESET);
			poll_val = poll(com_fd, com_count, -1);
			printf("\t\t\t\t\t\t\t\t%d\n\n\n", poll_val);
			CHECKSCEXIT(poll_val, true, "Error while polling");
			// if(poll_val == 0){ // inutile
			// 	// Check if a thread is not busy and assign work
			// 	for (size_t i = 0; i < configuration.workers; i++){
			// 		 if(free_threads[i]){
			// 			pthread_mutex_lock(&ready_queue_mtx);
			// 			if(ready_queue[0] != NULL){
			// 				pthread_mutex_unlock(&ready_queue);	
			// 				pthread_cond_signal(&client_is_ready);
			// 				continue;
			// 	 		}
			// 			pthread_mutex_unlock(&ready_queue);	
			// 		}
			// 	}
			// 	continue;
			// }
				
			}
			if(com_fd[0].revents & POLLIN){
				printf("\t\t\t\t\t\t\t\tSOCKET\n\n\n");

				com = accept(socket_fd, NULL, 0);
				CHECKERRNO((com < 0), "Errore durante la accept");
				for (size_t i = 0; i < configuration.workers; i++){
					 if(free_threads[i]){
						pthread_mutex_lock(&ready_queue_mtx);
						insert_client_ready_list(com, &ready_queue[0], &ready_queue[1]);
						pthread_mutex_unlock(&ready_queue_mtx);	
						pthread_cond_signal(&client_is_ready);
						break;
				 		}
						insert_client_ready_list(com, &ready_queue[0], &ready_queue[1]);
						pthread_mutex_unlock(&ready_queue_mtx);	
					}
				}	
				// pthread_mutex_lock(&pollfd_access);
				// insert_com_fd(com, &com_size, &com_count, com_fd);
				// pthread_mutex_unlock(&pollfd_access);
			
			if(com_fd[1].revents & POLLIN){
				printf("\t\t\t\t\t\t\t\tPIPE\n\n\n");

				read_bytes = read(m_w_pipe[0], buff, sizeof(buff));
				CHECKERRNO((read_bytes < 0), "Errore durante la lettura della pipe");
				tmp = atoi(buff);
				if(tmp < 0){
					fprintf(stderr, "Errore atoi! Buffer -> %s\n", buff);
					fflush(stderr);
					continue;
				}
				// pthread_mutex_lock(&pollfd_access);
				insert_com_fd(tmp, &com_size, &com_count, com_fd);
				continue;
				// pthread_mutex_unlock(&pollfd_access);
			}
				
			
			for(int i = 2; i < com_size; i++){
				puts("ciao");
				if(com_fd[i].revents & POLLIN){
					pthread_mutex_lock(&ready_queue_mtx);
					insert_client_ready_list(com_fd[i].fd, &ready_queue[0], &ready_queue[1]);
					pthread_mutex_lock(&ready_queue_mtx);
					com_fd[i].fd = -1;
					com_fd[i].events = 0;
					com_count--;
				}
					
			}
			puts("");
			for (size_t i = 0; i < configuration.workers; i++){
					 if(free_threads[i]){
						pthread_mutex_lock(&ready_queue_mtx);
						if(ready_queue[0] != NULL){
							pthread_mutex_unlock(&ready_queue_mtx);	
							pthread_cond_signal(&client_is_ready);
							continue;
				 		}
						pthread_mutex_unlock(&ready_queue_mtx);	
					}
				}	
		}
		
	
	
	close(socket_fd);
	clean_list(&ready_queue[0]);
	free(workers);
	return 0;

}





void signal_handler(int signum){
	
	if(signum == SIGHUP){
		puts(ANSI_COLOR_RED"Recived SIGHUP"ANSI_COLOR_RESET);
		can_accept = false;
	}

	else{
		if(signum == SIGQUIT)
			puts(ANSI_COLOR_RED"Recived SIGQUIT"ANSI_COLOR_RESET);
		if(signum == SIGINT)
			puts(ANSI_COLOR_RED"Recived SIGINT"ANSI_COLOR_RESET);
		abort_connections = true;
		can_accept = false;
	}

}




void printconf(const char* socketaddr){
	printf(ANSI_COLOR_GREEN CONF_LINE_TOP"│ %-12s\t"ANSI_COLOR_YELLOW"%20d"ANSI_COLOR_GREEN" │\n" CONF_LINE
			"│ %-12s\t"ANSI_COLOR_YELLOW"%20d"ANSI_COLOR_GREEN" │\n" CONF_LINE
			"│ %-12s\t"ANSI_COLOR_YELLOW"%20d"ANSI_COLOR_GREEN" │\n" CONF_LINE
			"│ %-12s\t"ANSI_COLOR_YELLOW"%20s"ANSI_COLOR_GREEN" │\n" CONF_LINE
			"│ %-12s\t"ANSI_COLOR_YELLOW"%20s"ANSI_COLOR_GREEN" │\n" CONF_LINE_BOTTOM"\n"ANSI_COLOR_RESET, "Workers:",
			configuration.workers, "Mem:", configuration.mem, "Files:", 
			configuration.files, "Socket file:", socketaddr, "Log:", configuration.log);
}
	
void init(char *sockname){
	
	FILE *conf = NULL;
	if((conf = fopen("bin/config.txt", "r")) == NULL){
		perror("Error while opening config file");
		exit(EXIT_FAILURE);
	}
	if(parseConfig(conf, &configuration) < 0){
		fprintf(stderr, ANSI_COLOR_RED "Error while parsing config.txt!\n" ANSI_COLOR_RESET);
		exit(EXIT_FAILURE);
	}
	fclose(conf);
	memset(sockname, 0 , UNIX_MAX_PATH);
	sprintf(sockname, "/tmp/");
	strncat(sockname, configuration.sockname, strlen(configuration.sockname));

}

void insert_com_fd(int com, nfds_t *size, nfds_t *count, struct pollfd *com_fd){
	int free_slot = 0;
	while(free_slot < *size && com_fd[free_slot].fd != -1){	free_slot++;}
	if(free_slot == *size)
		*size = realloc_com_fd(com_fd, free_slot);
	com_fd[free_slot].fd = com;
	com_fd[free_slot].events = POLLIN;
	*count += 1;
}

nfds_t realloc_com_fd(struct pollfd *com_fd, nfds_t free_slot){

	size_t new_size = free_slot*2;
	com_fd = realloc(com_fd, new_size);
	CHECKALLOC(com_fd, "Errore di riallocazione com_fd");
	for (size_t i = free_slot; i < new_size; i++){
		com_fd[i].fd = -1;
		com_fd[i].events = 0;
	}
	
	return new_size;

}

// static void remove_com_fd(int com, nfds_t *size, nfds_t *count, struct pollfd *com_fd){
// 	int i = 0;
// 	while(i < *size && com_fd[i].fd != com) i++;
// 	if(i == *size){
// 		fprintf(stderr, "Client non trovato, impossibile rimuovere!\n");
// 		fflush(stderr);
// 		return;
// 	}
// 	com_fd[i].fd = -1;
// 	com_fd[i].events = 0;
// 	*count--;
// }


