#include "server.h"
#include "parser.h"
#include "file.h"
#include "client_queue.h"
#include "log.h"
#define _GNU_SOURCE 
#define DEFAULTFDS 10



config configuration; // Server config
volatile sig_atomic_t can_accept = true;
volatile sig_atomic_t abort_connections = false;
pthread_mutex_t ready_queue_mtx = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t log_access_mtx = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t client_is_ready = PTHREAD_COND_INITIALIZER;
pthread_mutex_t abort_connections_mtx = PTHREAD_MUTEX_INITIALIZER;
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

void func(ready_clients *head){
	while (head != NULL){
		printf("%d -> ", head->com);
		head = head->next;
	
	}
	puts("NULL");
	
}

int main(int argc, char* argv[]){ // REMEMBER FFLUSH FOR THREAD PRINTF
	
	int socket_fd = 0, com = 0,  read_bytes = 0, tmp = 0, poll_val = 0, client_accepted = 0; // i = 0, ready_com = 0
	char buffer[PIPE_BUF]; // Buffer per inviare messaggi sullo stato dell'accettazione al client
	char SOCKETADDR[UNIX_MAX_PATH]; // Indirizzo del socket
	struct pollfd *com_fd =  (struct pollfd *) malloc(DEFAULTFDS*sizeof(struct pollfd));
	nfds_t com_count = 0;
	int i = 0;
	nfds_t com_size = DEFAULTFDS;
	ready_queue[0] = NULL;
	ready_queue[1] = NULL;
	CHECKALLOC(com_fd, "pollfd");
	
	pthread_t *workers;
	struct sockaddr_un sockaddress; // Socket init
	// unsigned int seed = time(NULL);
	
	init(SOCKETADDR); // Configuration struct is now initialized
	open_log(configuration.log);
	PRINT_WELCOME;
	printconf(SOCKETADDR);
	// Signal handler
	struct sigaction sig; 
	memset(&sig, 0, sizeof(sig));
	sig.sa_handler = signal_handler;
	sigaction(SIGINT,&sig,NULL);
	sigaction(SIGHUP,&sig,NULL);
	sigaction(SIGQUIT,&sig,NULL);
	// END signal handler
	write_to_log("Signal_handler installato.");

	
	write_to_log("Inizializzo i workers.");

	workers = (pthread_t *) malloc(configuration.workers*sizeof(pthread_t)); // Pool di workers
	CHECKALLOC(workers, "workers array");
	memset(workers, 0, configuration.workers*sizeof(pthread_t));
	write_to_log("Workers array inizializzato.");

	free_threads = (bool *) malloc(configuration.workers*sizeof(bool));
	memset(free_threads, true, configuration.workers*sizeof(bool));
	CHECKALLOC(free_threads, "workers array");

	memset(com_fd, -1, sizeof(struct pollfd));
	memset(buffer, 0, PIPE_BUF);
	CHECKEXIT((pipe(m_w_pipe) == -1), true, "Impossibile inizializzare la pipe");
	write_to_log("Pipe inzializzata.");


	strncpy(sockaddress.sun_path, SOCKETADDR, UNIX_MAX_PATH);
	sockaddress.sun_family = AF_UNIX;
	socket_fd = socket(AF_UNIX, SOCK_STREAM, 0);
	unlink(SOCKETADDR);
	CHECKSCEXIT(bind(socket_fd, (struct sockaddr *) &sockaddress, sizeof(sockaddress)), true, "Non sono riuscito a fare la bind");
	CHECKSCEXIT(listen(socket_fd, 10), true, "Impossibile effettuare la listen");
	write_to_log("Ho inizializzato il socket ed ho eseguito la bind e la listen.");


	com_fd[0].fd = socket_fd;
	com_fd[0].events = POLLIN;
	com_fd[1].fd = m_w_pipe[0];
	com_fd[1].events = POLLIN;
	com_count = 2;
	write_to_log("Polling struct inizializzata con il socket_fd su i = 0 e l'endpoint della pipe su i = 1.");
	// printf(ANSI_COLOR_MAGENTA">> Partito Thread");
	for (int i = 0; i < configuration.workers; i++){
		pthread_create(&workers[i], NULL, &worker, &i);
		// pthread_detach(workers[i]);
	}
	puts(ANSI_COLOR_RESET);
	while(true){
		puts(ANSI_COLOR_GREEN"Polling..."ANSI_COLOR_RESET);
		poll_val = poll(com_fd, com_count, -1);
		if(can_accept){	
			CHECKSCEXIT(poll_val, true, "Error while polling");
			
			
			if(com_fd[0].revents & POLLIN){
				com = accept(socket_fd, NULL, 0);
				client_accepted++;

				CHECKERRNO((com < 0), "Errore durante la accept");
				for (size_t i = 0; i < configuration.workers; i++){
					pthread_mutex_lock(&free_threads_mtx);
					 if(free_threads[i]){
						pthread_mutex_unlock(&free_threads_mtx);

						pthread_mutex_lock(&ready_queue_mtx);
						insert_client_ready_list(com, &ready_queue[0], &ready_queue[1]);
						pthread_cond_signal(&client_is_ready);
						pthread_mutex_unlock(&ready_queue_mtx);	
						break;
				 		}
						pthread_mutex_unlock(&free_threads_mtx);
						pthread_mutex_lock(&ready_queue_mtx);
						insert_client_ready_list(com, &ready_queue[0], &ready_queue[1]);
						pthread_mutex_unlock(&ready_queue_mtx);	
					}
				}	
			
			if(com_fd[1].revents & POLLIN){
				
				read_bytes = read(m_w_pipe[0], buffer, sizeof(buffer));
				
				
				
				CHECKERRNO((read_bytes < 0), "Errore durante la lettura della pipe");
				tmp = strtol(buffer, NULL, 10);
				if(tmp <= 0){
					fprintf(stderr, "Errore strtol! Buffer -> %s\n", buffer);
					fflush(stderr);
					continue;
				}
				if (com_size - com_count < 3){
					com_size = realloc_com_fd(&com_fd, com_size);
					for (size_t i = com_count; i < com_size; i++){
						com_fd[i].fd = 0;
						com_fd[i].events = 0;
					}
				}
				insert_com_fd(tmp, &com_size, &com_count, com_fd);
			}
				
			
			for(int i = 2; i < com_size; i++){
				if((com_fd[i].revents & POLLIN) && com_fd[i].fd != 0){
					pthread_mutex_lock(&ready_queue_mtx);
					insert_client_ready_list(com_fd[i].fd, &ready_queue[0], &ready_queue[1]);
					pthread_mutex_unlock(&ready_queue_mtx);
					com_fd[i].fd = 0;
					com_fd[i].events = 0;
					com_count--;
				}
					
			}
			for (size_t i = 0; i < configuration.workers; i++){
				pthread_mutex_lock(&free_threads_mtx);
				if(free_threads[i]){
					pthread_mutex_unlock(&free_threads_mtx);
					pthread_mutex_lock(&ready_queue_mtx);
					if(ready_queue[0] != NULL){
						pthread_cond_signal(&client_is_ready);
						pthread_mutex_unlock(&ready_queue_mtx);	
						
						break;
					}
					else {
						pthread_mutex_unlock(&ready_queue_mtx);	
						break;
					}
				}
				pthread_mutex_unlock(&free_threads_mtx);	
	

			}
		}
		
		else{
			if(abort_connections){
				for(size_t i = 0; i < com_size; i++){
					if (com_fd[i].fd != 0)
						close(com_fd[i].fd);
				}
			}
			else{
				while(com_count != 0 && ready_queue[0] != NULL){
					poll_val = poll(com_fd, com_count, -1);
					if(com_fd[1].revents & POLLIN){
					read_bytes = read(m_w_pipe[0], buffer, sizeof(buffer));
					CHECKERRNO((read_bytes < 0), "Errore durante la lettura della pipe");
					tmp = strtol(buffer, NULL, 10);
					if(tmp <= 0){
						fprintf(stderr, "Errore strtol! Buffer -> %s\n", buffer);
						fflush(stderr);
						continue;
					}
					if (com_size - com_count < 3){
						com_size = realloc_com_fd(&com_fd, com_size);
						for (size_t i = com_count; i < com_size; i++){
							com_fd[i].fd = 0;
							com_fd[i].events = 0;
						}
					}
					insert_com_fd(tmp, &com_size, &com_count, com_fd);
					}
					for(int i = 2; i < com_size; i++){
						if((com_fd[i].revents & POLLIN) && com_fd[i].fd != 0){
							pthread_mutex_lock(&ready_queue_mtx);
							insert_client_ready_list(com_fd[i].fd, &ready_queue[0], &ready_queue[1]);
							pthread_mutex_unlock(&ready_queue_mtx);
							com_fd[i].fd = 0;
							com_fd[i].events = 0;
							com_count--;
						}
						
					}
					for (size_t i = 0; i < configuration.workers; i++){
						pthread_mutex_lock(&free_threads_mtx);
						if(free_threads[i]){
							pthread_mutex_unlock(&free_threads_mtx);
							pthread_mutex_lock(&ready_queue_mtx);
							if(ready_queue[0] != NULL){
								pthread_cond_signal(&client_is_ready);
								pthread_mutex_unlock(&ready_queue_mtx);	
								
								break;
							}
							else {
								pthread_mutex_unlock(&ready_queue_mtx);	
								break;
							}
						}
					pthread_mutex_unlock(&free_threads_mtx);	
					}
				}
				
			}
			break;
		}
	}
	for(size_t i = 0; i < configuration.workers; i++)
		pthread_join(workers[i], NULL);
	close_log();
	
	close(socket_fd);
	puts("socket closed");

	clean_list(&ready_queue[0]);
	puts("listclosed");
	free(workers);
	puts("workers closed");
	free(com_fd);
	puts("comfd closed");
	return 0;

}






void signal_handler(int signum){
	
	if(signum == SIGHUP){
		puts(ANSI_COLOR_RED"Received SIGHUP"ANSI_COLOR_RESET);
		can_accept = false;
	}

	else{
		if(signum == SIGQUIT)
			puts(ANSI_COLOR_RED"Received SIGQUIT"ANSI_COLOR_RESET);
		if(signum == SIGINT)
			puts(ANSI_COLOR_RED"Received SIGINT"ANSI_COLOR_RESET);
			
			
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
	while(free_slot < *size && com_fd[free_slot].fd != 0) free_slot++;
	
	// printf("com %d\n", com);
	com_fd[free_slot].fd = com;
	com_fd[free_slot].events = POLLIN;
	*count += 1;
// 	printf("Da insert: ");
// 	for (size_t i = 0; i < *size; i++)
// 		printf("%d ", com_fd[i].fd);

// 	puts("");	
}

nfds_t realloc_com_fd(struct pollfd **com_fd, nfds_t free_slot){
	size_t new_size = free_slot + DEFAULTFDS;
	*com_fd = (struct pollfd* )realloc(*com_fd, new_size*sizeof(struct pollfd));
	CHECKALLOC(com_fd, "Errore di riallocazione com_fd");
	
	
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


