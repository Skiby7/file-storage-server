#include "server.h"
#include "parser.h"
#include "file.h"
#include "client_queue.h"
#include "log.h"
#define DEFAULTFDS 10



config configuration; // Server config
bool can_accept = true;
bool abort_connections = false;
pthread_mutex_t ready_queue_mtx = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t log_access_mtx = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t client_is_ready = PTHREAD_COND_INITIALIZER;
pthread_mutex_t abort_connections_mtx = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t can_accept_mtx = PTHREAD_MUTEX_INITIALIZER;
bool *free_threads;
ready_clients *ready_queue[2];
int m_w_pipe[2]; // 1 lettura, 0 scrittura
extern void* worker(void* args);
pthread_mutex_t free_threads_mtx = PTHREAD_MUTEX_INITIALIZER;

/** TODO:
 * - Riguardare pthread join per far terminare i thread
 * 
 * 
 * 
 * 
 * 
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
	nfds_t com_size = DEFAULTFDS;
	ready_queue[0] = NULL;
	ready_queue[1] = NULL;
	CHECKALLOC(com_fd, "pollfd");
	bool thread_finished = false;
	pthread_t *workers;
	pthread_t signal_handler_thread;
	sigset_t signal_mask;
	struct sockaddr_un sockaddress; // Socket init
	
	init(SOCKETADDR); // Configuration struct is now initialized
	open_log(configuration.log);
	PRINT_WELCOME;
	printconf(SOCKETADDR);
	// Signal handler
	CHECKSCEXIT(sigfillset(&signal_mask), true, "Errore durante il settaggio di signal_mask");
	CHECKSCEXIT(sigdelset(&signal_mask, SIGSEGV), true, "Errore durante il settaggio di signal_mask");
	CHECKSCEXIT(sigdelset(&signal_mask, SIGPIPE), true, "Errore durante il settaggio di signal_mask");
	CHECKEXIT(pthread_sigmask(SIG_SETMASK, &signal_mask, NULL) != 0, false, "Errore durante il mascheramento dei segnali");
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
	CHECKEXIT(pthread_create(&signal_handler_thread, NULL, &sig_wait_thread, NULL) != 0, false, "Errore di creazione del signal handler thread");

	for (int i = 0; i < configuration.workers; i++){
		CHECKEXIT(pthread_create(&workers[i], NULL, &worker, &i), false, "Errore di creazione dei worker");
	}
	puts(ANSI_COLOR_RESET);
	while(true){
		puts(ANSI_COLOR_GREEN"Polling..."ANSI_COLOR_RESET);
		poll_val = poll(com_fd, com_count, -1);
		CHECKERRNO(poll_val < 0, "Errore durante il polling");
		pthread_mutex_lock(&abort_connections_mtx);
		if(abort_connections){
			pthread_mutex_unlock(&abort_connections_mtx);
			break;
		}
		pthread_mutex_unlock(&abort_connections_mtx);


		pthread_mutex_lock(&can_accept_mtx);
		pthread_mutex_lock(&ready_queue_mtx);
		if(!can_accept && com_count == 2 && ready_queue[0] == NULL){
			printf(ANSI_COLOR_RED"CAN ACCEPT: %d\nCOM_COUNT: %ld\n"ANSI_COLOR_RESET, can_accept, com_count);
			if(ready_queue[0] == NULL)
				printf(ANSI_COLOR_RED"QUEUE EMPTY\n"ANSI_COLOR_RESET);
			pthread_mutex_unlock(&ready_queue_mtx);
			pthread_mutex_unlock(&can_accept_mtx);
			for (size_t i = 0; i < configuration.workers; i++){
				printf(ANSI_COLOR_RED"%d "ANSI_COLOR_RESET, free_threads[i]);
				pthread_mutex_lock(&free_threads_mtx);
				if(!free_threads[i]){
					pthread_mutex_unlock(&free_threads_mtx);
					break;
				}
				if(i == configuration.workers - 1 && free_threads[i])
					thread_finished = true;
				pthread_mutex_unlock(&free_threads_mtx);
				
					
			}
			puts("");
			printf(ANSI_COLOR_RED"THREAD FINISHED %d\n"ANSI_COLOR_RESET, thread_finished);
			if(thread_finished)
				break;
			continue;
		}
		else{
			pthread_mutex_unlock(&ready_queue_mtx);
			pthread_mutex_unlock(&can_accept_mtx);
		}
		


		if(com_fd[0].revents & POLLIN){
			com = accept(socket_fd, NULL, 0);
			pthread_mutex_lock(&can_accept_mtx);
			if(!can_accept){
				pthread_mutex_unlock(&can_accept_mtx);
				close(com);
			}
			else{
				pthread_mutex_unlock(&can_accept_mtx);
				CHECKERRNO((com < 0), "Errore durante la accept");
				client_accepted++;
				for (size_t i = 0; i < configuration.workers; i++){
					pthread_mutex_lock(&free_threads_mtx);
					if(free_threads[i]){ // Se ho un thread libero gli assegno subito il lavoro e continuo il ciclo
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
		}	
		
		if(com_fd[1].revents & POLLIN){
			read_bytes = read(m_w_pipe[0], buffer, sizeof(buffer));
			CHECKERRNO((read_bytes < 0), "Errore durante la lettura della pipe");
			if(strncmp(buffer, "termina", PIPE_BUF) != 0){
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
		}
			
		for(size_t i = 2; i < com_size; i++){
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
					
					// break; CHANGED -> I WANT TO ASSIGN AS MUCH WORK AS I CAN
				}
				else{
					pthread_mutex_unlock(&ready_queue_mtx);
					break;	// HERE I WANT TO STOP AS THE QUEUE IS EMPTY
				}
			}
			pthread_mutex_unlock(&free_threads_mtx);
		}
	

	}
	pthread_mutex_lock(&log_access_mtx);
	write_to_log("Server stopped");
	pthread_mutex_unlock(&log_access_mtx);
	close_log();
	
	pthread_mutex_lock(&abort_connections_mtx);
	if(!abort_connections){
		abort_connections = true;
		pthread_mutex_unlock(&abort_connections_mtx);
	}
	pthread_mutex_unlock(&abort_connections_mtx);
	pthread_mutex_lock(&ready_queue_mtx);
	pthread_cond_broadcast(&client_is_ready); // sveglio tutti i thread
	pthread_mutex_unlock(&ready_queue_mtx);
	for (int i = 0; i < configuration.workers; i++){
		CHECKEXIT(pthread_join(workers[i], NULL) != 0, false, "Errore durante il join dei workes");
	}
	CHECKEXIT(pthread_join(signal_handler_thread, NULL) != 0, false, "Errore durante il join dei workes");
	for (size_t i = 0; i < com_size; i++){
			if(com_fd[i].fd != 0)
				close(com_fd[i].fd);
		}
	close(socket_fd);
	close(m_w_pipe[0]);
	close(m_w_pipe[1]);
	puts("socket closed");
	freeConfig(&configuration);
	clean_list(&ready_queue[0]);
	puts("list closed");
	free(workers);
	puts("workers closed");
	free(com_fd);
	free(free_threads);
	puts("comfd closed");
	clean_list(&ready_queue[0]);

	return 0;
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
	com_fd[free_slot].fd = com;
	com_fd[free_slot].events = POLLIN;
	*count += 1;
}

nfds_t realloc_com_fd(struct pollfd **com_fd, nfds_t free_slot){
	size_t new_size = free_slot + DEFAULTFDS;
	*com_fd = (struct pollfd* )realloc(*com_fd, new_size*sizeof(struct pollfd));
	CHECKALLOC(com_fd, "Errore di riallocazione com_fd");
	return new_size;
}

void* sig_wait_thread(void *args){
	int signum = 0;
	sigset_t sig_set;
	char buffer[PIPE_BUF];
	memset(buffer, 0, PIPE_BUF);
	CHECKSCEXIT(sigemptyset(&sig_set), true, "Errore di inizializzazione sig_set");
	CHECKSCEXIT(sigaddset(&sig_set, SIGINT), true, "Errore di inizializzazione sig_set");
	CHECKSCEXIT(sigaddset(&sig_set, SIGHUP), true, "Errore di inizializzazione sig_set");
	CHECKSCEXIT(sigaddset(&sig_set, SIGQUIT), true, "Errore di inizializzazione sig_set");
	while(true){
		pthread_mutex_lock(&log_access_mtx);
		write_to_log("Avviato signal handler thread");
		pthread_mutex_unlock(&log_access_mtx);
		CHECKEXIT(sigwait(&sig_set, &signum) != 0, false, "Errore sigwait");
		if(signum == SIGINT || signum == SIGQUIT){
			pthread_mutex_lock(&abort_connections_mtx);
			abort_connections = true;
			pthread_mutex_unlock(&abort_connections_mtx);
			pthread_mutex_lock(&can_accept_mtx);
			can_accept = false;
			pthread_mutex_unlock(&can_accept_mtx);
			sprintf(buffer, "termina");
			CHECKERRNO((write(m_w_pipe[1], buffer, sizeof(buffer)) < 0), "Errore invio terminazione sulla pipe");
			break;
		}
		if(signum == SIGHUP){
			pthread_mutex_lock(&can_accept_mtx);
			can_accept = false;
			pthread_mutex_unlock(&can_accept_mtx);
			sprintf(buffer, "termina");
			CHECKERRNO((write(m_w_pipe[1], buffer, sizeof(buffer)) < 0), "Errore invio terminazione sulla pipe");
			break;
		}
	}
	return (void *) 0;
}