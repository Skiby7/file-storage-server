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
clients_list *ready_queue[2];
pthread_mutex_t lines_mtx = PTHREAD_MUTEX_INITIALIZER;
int lines = 21;
int good_fd_pipe[2]; // 1 lettura, 0 scrittura
int done_fd_pipe[2]; // 1 lettura, 0 scrittura
extern void* worker(void* args);
pthread_mutex_t free_threads_mtx = PTHREAD_MUTEX_INITIALIZER;

extern storage server_storage;
extern pthread_cond_t start_LFU_selector;




void func(clients_list *head){
	while (head != NULL){
		printf("%d -> ", head->com);
		head = head->next;
	
	}
	puts("NULL");
	
}

void print_textual_ui(char* SOCKETADDR){
	SAFELOCK(lines_mtx);
	if(lines > 20){
		lines = 0;
		printf(ANSI_CLEAR_SCREEN);
		PRINT_WELCOME;
		printconf(SOCKETADDR);
	}
	SAFEUNLOCK(lines_mtx);
}

int main(int argc, char* argv[]){
	if(argc != 2){
		fprintf(stderr, "Usare %s path/to/config\n", argv[0]);
		exit(EXIT_FAILURE);
	}
	
	int socket_fd = 0, com = 0,  read_bytes = 0, tmp = 0, poll_val = 0, client_accepted = 0, client_closed = 0; // i = 0, ready_com = 0
	char buffer[PIPE_BUF]; // Buffer per inviare messaggi sullo stato dell'accettazione al client
	char SOCKETADDR[AF_UNIX_MAX_PATH]; // Indirizzo del socket
	struct pollfd *com_fd =  (struct pollfd *) malloc(DEFAULTFDS*sizeof(struct pollfd));
	nfds_t com_count = 0;
	nfds_t com_size = DEFAULTFDS;
	ready_queue[0] = NULL;
	ready_queue[1] = NULL;


	CHECKALLOC(com_fd, "pollfd");
	bool thread_finished = false;
	pthread_t *workers;
	pthread_t signal_handler_thread;
	pthread_t use_stat_thread;
	sigset_t signal_mask;
	struct sockaddr_un sockaddress; // Socket init
	
	init(SOCKETADDR, argv[1]); // Configuration struct is now initialized
	open_log(configuration.log);
	
	// Signal handler
	CHECKSCEXIT(sigfillset(&signal_mask), true, "Errore durante il settaggio di signal_mask");
	CHECKSCEXIT(sigdelset(&signal_mask, SIGSEGV), true, "Errore durante il settaggio di signal_mask");
	// CHECKSCEXIT(sigdelset(&signal_mask, SIGPIPE), true, "Errore durante il settaggio di signal_mask");
	CHECKEXIT(pthread_sigmask(SIG_SETMASK, &signal_mask, NULL) != 0, false, "Errore durante il mascheramento dei segnali");
	// END signal handler
	write_to_log("Segnali mascherati.");

	init_table(configuration.files, configuration.mem);
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
	CHECKEXIT((pipe(good_fd_pipe) == -1), true, "Impossibile inizializzare la pipe");
	CHECKEXIT((pipe(done_fd_pipe) == -1), true, "Impossibile inizializzare la pipe");
	write_to_log("Pipe inzializzata.");

	strncpy(sockaddress.sun_path, SOCKETADDR, AF_UNIX_MAX_PATH-1);
	sockaddress.sun_family = AF_UNIX;
	socket_fd = socket(AF_UNIX, SOCK_STREAM, 0);
	unlink(SOCKETADDR);
	CHECKSCEXIT(bind(socket_fd, (struct sockaddr *) &sockaddress, sizeof(sockaddress)), true, "Non sono riuscito a fare la bind");
	CHECKSCEXIT(listen(socket_fd, 10), true, "Impossibile effettuare la listen");
	write_to_log("Ho inizializzato il socket ed ho eseguito la bind e la listen.");

	com_fd[0].fd = socket_fd;
	com_fd[0].events = POLLIN;
	com_fd[1].fd = good_fd_pipe[0];
	com_fd[1].events = POLLIN;
	com_fd[2].fd = done_fd_pipe[0];
	com_fd[2].events = POLLIN;
	com_count = 3;
	write_to_log("Polling struct inizializzata con il socket_fd su i = 0 e l'endpoint della pipe su i = 1.");
	CHECKEXIT(pthread_create(&signal_handler_thread, NULL, &sig_wait_thread, NULL) != 0, false, "Errore di creazione del signal handler thread");
	CHECKEXIT(pthread_create(&use_stat_thread, NULL, &use_stat_update, NULL) != 0, false, "Errore di creazione di use stat thread");
	for (int i = 0; i < configuration.workers; i++){
		CHECKEXIT(pthread_create(&workers[i], NULL, &worker, &i), false, "Errore di creazione dei worker");
	}

	while(true){
		if(configuration.tui) print_textual_ui(SOCKETADDR);
		
		
		poll_val = poll(com_fd, com_count, -1);
		CHECKERRNO(poll_val < 0, "Errore durante il polling");
		// PRINT_POLLING(poll_print);
		// printf("poll_val -> %d\n", poll_val);
		SAFELOCK(abort_connections_mtx);
		if(abort_connections){
			SAFEUNLOCK(abort_connections_mtx);
			break;
		}
		SAFEUNLOCK(abort_connections_mtx);
		
		if(com_fd[0].revents & POLLIN){
			com = accept(socket_fd, NULL, 0);
			SAFELOCK(can_accept_mtx);
			if(!can_accept){
				SAFEUNLOCK(can_accept_mtx);
				close(com);
			}
			else if(com < 0){
				SAFEUNLOCK(can_accept_mtx);
				CHECKERRNO(true, "Errore durante la accept");
			}
			else{
				SAFEUNLOCK(can_accept_mtx);
				client_accepted++;
				if (com_size - com_count < 3){
						com_size = realloc_com_fd(&com_fd, com_size);
						for (size_t i = com_count; i < com_size; i++){
							com_fd[i].fd = 0;
							com_fd[i].events = 0;
						}
					}
				insert_com_fd(com, &com_size, &com_count, com_fd);
			}
		}	
			
		if(com_fd[1].revents & POLLIN){
			// printf("REVENTS %d \n", com_fd[2].revents);

			read_bytes = read(good_fd_pipe[0], buffer, sizeof(buffer));
			CHECKERRNO((read_bytes < 0), "Errore durante la lettura della pipe");
			if(strncmp(buffer, "termina", PIPE_BUF) != 0){
				tmp = strtol(buffer, NULL, 10);
				if(tmp <= 0)
					fprintf(stderr, "Errore strtol good_pipe! Buffer -> %s\n", buffer);
					
				else{
					// printf("ARRIVED %d in good_pipe\n", tmp);

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
		}
			
		if(com_fd[2].revents & POLLIN){
			// printf("REVENTS %d \n", com_fd[2].revents);
			read_bytes = read(done_fd_pipe[0], buffer, sizeof buffer);
			CHECKERRNO((read_bytes < 0), "Errore durante la lettura della pipe");
			
			tmp = strtol(buffer, NULL, 10);
			if(tmp <= 0)
				fprintf(stderr, "Errore strtol done_pipe! Buffer -> %s\n", buffer);
			else{
				// shutdown(tmp, SHUT_RDWR);
				// printf("ARRIVED %d in done_pipe\n", tmp);

				CHECKERRNO(close(tmp) < 0, "Errore chiusura done queue pipe");
				client_closed++;
				continue;
			}
		}
			
		for(size_t i = 3; i < com_size; i++){
			if((com_fd[i].revents & POLLIN) && com_fd[i].fd != 0){
				SAFELOCK(ready_queue_mtx);
				insert_client_list(com_fd[i].fd, &ready_queue[0], &ready_queue[1]);
				SAFEUNLOCK(ready_queue_mtx);
				com_fd[i].fd = 0;
				com_fd[i].events = 0;
				com_count--;
			}
		}
		

		for (size_t i = 0; i < configuration.workers; i++){
			SAFELOCK(free_threads_mtx);
			if(free_threads[i]){
				SAFEUNLOCK(free_threads_mtx);
				SAFELOCK(ready_queue_mtx);
				if(ready_queue[0] != NULL){
					pthread_cond_signal(&client_is_ready);
					SAFEUNLOCK(ready_queue_mtx);	
					continue; 
				}
				else{
					SAFEUNLOCK(ready_queue_mtx);
					break;
				}
			}
			SAFEUNLOCK(free_threads_mtx);
		}
		
		SAFELOCK(can_accept_mtx);
		if(!can_accept && client_accepted == client_closed){
			puts("waiting");
			SAFEUNLOCK(can_accept_mtx);
			while (!thread_finished){
				for (size_t i = 0; i < configuration.workers; i++){
					SAFELOCK(free_threads_mtx);
					if (!free_threads[i]){
						SAFEUNLOCK(free_threads_mtx);
						break;
					}
					if (free_threads[i] && i == configuration.workers - 1)
						thread_finished = true;
					SAFEUNLOCK(free_threads_mtx);
					
				}
				sleep(1);
			}
			break;
		}
		SAFEUNLOCK(can_accept_mtx);
	}
	SAFELOCK(log_access_mtx);
	write_to_log("Server stopped");
	SAFEUNLOCK(log_access_mtx);
	close_log();
	
	SAFELOCK(abort_connections_mtx);
	if(!abort_connections){
		abort_connections = true;
		SAFEUNLOCK(abort_connections_mtx);
	}
	else
		SAFEUNLOCK(abort_connections_mtx);
	SAFELOCK(ready_queue_mtx);
	pthread_cond_broadcast(&client_is_ready); // sveglio tutti i thread
	SAFEUNLOCK(ready_queue_mtx);
	
	
	for (int i = 0; i < configuration.workers; i++){
		CHECKEXIT(pthread_join(workers[i], NULL) != 0, false, "Errore durante il join dei workers");
	}
	CHECKEXIT(pthread_join(signal_handler_thread, NULL) != 0, false, "Errore durante il join dei workers");
	SAFELOCK(server_storage.storage_access_mtx);
	pthread_cond_broadcast(&start_LFU_selector); // sveglio tutti i thread
	SAFEUNLOCK(server_storage.storage_access_mtx);
	CHECKEXIT(pthread_join(use_stat_thread, NULL) != 0, false, "Errore durante la cancellazione dei workers attivi");
	for (size_t i = 0; i < com_size; i++){
			if(com_fd[i].fd != 0)
				close(com_fd[i].fd);
	}
	
	close(socket_fd);
	close(good_fd_pipe[0]);
	close(good_fd_pipe[1]);
	close(done_fd_pipe[0]);
	close(done_fd_pipe[1]);
	puts("socket closed");
	print_summary();
	freeConfig(&configuration);
	clean_storage();
	clean_ready_list(&ready_queue[0]);
	puts("list closed");
	free(workers);
	puts("workers closed");
	free(com_fd);
	free(free_threads);
	puts("comfd closed");
	pthread_mutex_destroy(&lines_mtx);
	pthread_mutex_destroy(&ready_queue_mtx);
	pthread_mutex_destroy(&can_accept_mtx);
	pthread_mutex_destroy(&log_access_mtx);
	pthread_mutex_destroy(&free_threads_mtx);
	pthread_cond_destroy(&client_is_ready);

	return 0;
}

void printconf(const char* socketaddr){
	// if(!configuration.summary){
		printf(ANSI_COLOR_GREEN CONF_LINE_TOP
			"│ %-12s\t"ANSI_COLOR_YELLOW"%20d"ANSI_COLOR_GREEN" │\n" CONF_LINE
			"│ %-12s\t"ANSI_COLOR_YELLOW"%20d"ANSI_COLOR_GREEN" │\n" CONF_LINE
			"│ %-12s\t"ANSI_COLOR_YELLOW"%20d"ANSI_COLOR_GREEN" │\n" CONF_LINE
			"│ %-12s\t"ANSI_COLOR_YELLOW"%20s"ANSI_COLOR_GREEN" │\n" CONF_LINE
			"│ %-12s\t"ANSI_COLOR_YELLOW"%20s"ANSI_COLOR_GREEN" │\n" CONF_LINE_BOTTOM"\n"ANSI_COLOR_RESET, 
			"Workers:",	configuration.workers, "Mem:", configuration.mem, "Files:", 
			configuration.files, "Socket file:", socketaddr, "Log:", configuration.log);
			return;
	// }
	// printf(ANSI_COLOR_GREEN CONF_LINE_TOP
	// 		"│ %-12s\t"ANSI_COLOR_YELLOW"%20d"ANSI_COLOR_GREEN" │\n" CONF_LINE, "Workers:",	configuration.workers);
	// 		// "│ %-12s\t"ANSI_COLOR_YELLOW"%20d"ANSI_COLOR_GREEN" │\n" CONF_LINE
	// 		// "│ %-12s\t"ANSI_COLOR_YELLOW"%20d"ANSI_COLOR_GREEN" │\n" CONF_LINE
	// print_storage_info();
	// printf("│ %-12s\t"ANSI_COLOR_YELLOW"%20s"ANSI_COLOR_GREEN" │\n" CONF_LINE
	// 		"│ %-12s\t"ANSI_COLOR_YELLOW"%20s"ANSI_COLOR_GREEN" │\n" CONF_LINE_BOTTOM"\n"ANSI_COLOR_RESET, 
	// 		"Socket file:", socketaddr, "Log:", configuration.log);
}
	
void init(char *sockname, char *config_file){
	
	FILE *conf = NULL;
	if((conf = fopen(config_file, "r")) == NULL){
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
	strncat(sockname, configuration.sockname, AF_UNIX_MAX_PATH-1);
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
	SAFELOCK(log_access_mtx);
	write_to_log("Avviato signal handler thread");
	SAFEUNLOCK(log_access_mtx);
	CHECKSCEXIT(sigemptyset(&sig_set), true, "Errore di inizializzazione sig_set");
	CHECKSCEXIT(sigaddset(&sig_set, SIGINT), true, "Errore di inizializzazione sig_set");
	CHECKSCEXIT(sigaddset(&sig_set, SIGHUP), true, "Errore di inizializzazione sig_set");
	CHECKSCEXIT(sigaddset(&sig_set, SIGQUIT), true, "Errore di inizializzazione sig_set");
	while(true){
		CHECKEXIT(sigwait(&sig_set, &signum) != 0, false, "Errore sigwait");
		if(signum == SIGINT || signum == SIGQUIT){
			SAFELOCK(abort_connections_mtx);
			abort_connections = true;
			SAFEUNLOCK(abort_connections_mtx);
			SAFELOCK(can_accept_mtx);
			can_accept = false;
			SAFEUNLOCK(can_accept_mtx);
			sprintf(buffer, "termina");
			CHECKERRNO((write(good_fd_pipe[1], buffer, sizeof(buffer)) < 0), "Errore invio terminazione sulla pipe");
			break;
		}
		if(signum == SIGHUP){
			SAFELOCK(can_accept_mtx);
			can_accept = false;
			SAFEUNLOCK(can_accept_mtx);
			sprintf(buffer, "termina");
			CHECKERRNO((write(good_fd_pipe[1], buffer, sizeof(buffer)) < 0), "Errore invio terminazione sulla pipe");
			break;
		}
	}
	return (void *) 0;
}
