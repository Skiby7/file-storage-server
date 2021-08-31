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

int good_fd_pipe[2]; // 1 lettura, 0 scrittura
int done_fd_pipe[2]; // 1 lettura, 0 scrittura
int tui_pipe[2];
void* sig_wait_thread(void *args);
extern void* worker(void* args);
extern void* print_tui(void* args);
pthread_mutex_t free_threads_mtx = PTHREAD_MUTEX_INITIALIZER;

extern fss_storage_t server_storage;
extern pthread_cond_t start_victim_selector;

void func(clients_list *head){
	while (head != NULL){
		printf("%d -> ", head->com);
		head = head->next;
	
	}
	puts("NULL");
	
}

void print_tui_header(char* SOCKETADDR){
	printf(ANSI_CLEAR_SCREEN);
	PRINT_WELCOME;
	printconf(SOCKETADDR);
	puts("\n\n");
}

int main(int argc, char* argv[]){
	if(argc != 2){
		fprintf(stderr, "Usare %s path/to/config\n", argv[0]);
		exit(EXIT_FAILURE);
	}
	int socket_fd = 0, com = 0,  read_bytes = 0, tmp = 0, poll_val = 0, clients_active = 0, max_clients_active = 0; 
	bool threads_finished = false; // i = 0, ready_com = 0
	char buffer[PIPE_BUF] = {0}; // Buffer per inviare messaggi sullo stato dell'accettazione al client
	char SOCKETADDR[AF_UNIX_MAX_PATH] = {0}; // Indirizzo del socket
	char log_buffer[200] = {0};
	struct pollfd *com_fd =  (struct pollfd *) malloc(DEFAULTFDS*sizeof(struct pollfd));
	CHECKALLOC(com_fd);
	nfds_t com_count = 0;
	nfds_t com_size = DEFAULTFDS;
	ready_queue[0] = NULL;
	ready_queue[1] = NULL;

	pthread_t *workers;
	pthread_t signal_handler_thread;
	pthread_t use_stat_thread;
	pthread_t tui_thread;
	
	sigset_t signal_mask;
	struct sockaddr_un sockaddress; // Socket init
	
	CHECKSCEXIT(sigfillset(&signal_mask), true, "Errore durante il settaggio di signal_mask");
	CHECKSCEXIT(sigdelset(&signal_mask, SIGSEGV), true, "Errore durante il settaggio di signal_mask");
	// CHECKSCEXIT(sigdelset(&signal_mask, SIGPIPE), true, "Errore durante il settaggio di signal_mask");
	CHECKEXIT(pthread_sigmask(SIG_SETMASK, &signal_mask, NULL) != 0, false, "Errore durante il mascheramento dei segnali");
	
	init(SOCKETADDR, argv[1]); // Configuration struct is now initialized
	
	open_log(configuration.log);
	write_to_log("Segnali mascherati.");

	init_table(configuration.files, configuration.mem, configuration.compression, configuration.compression_level, configuration.replacement_algo);
	write_to_log("Inizializzo i workers.");

	workers = (pthread_t *) malloc(configuration.workers*sizeof(pthread_t)); // Pool di workers
	CHECKALLOC(workers);
	write_to_log("Workers array inizializzato.");

	free_threads = (bool *) malloc(configuration.workers*sizeof(bool));
	CHECKALLOC(free_threads);
	memset(free_threads, true, configuration.workers*sizeof(bool));

	memset(com_fd, -1, sizeof(struct pollfd));
	CHECKSCEXIT((pipe(good_fd_pipe)), true, "Impossibile inizializzare la pipe");
	CHECKSCEXIT((pipe(done_fd_pipe)), true, "Impossibile inizializzare la pipe");
	CHECKSCEXIT((pipe(tui_pipe)), true, "Impossibile inizializzare la pipe");
	write_to_log("Pipe inzializzate.");

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

	CHECKEXIT(pthread_create(&signal_handler_thread, NULL, &sig_wait_thread, NULL) != 0, false, "Errore di creazione del signal handler thread");
	CHECKEXIT(pthread_create(&use_stat_thread, NULL, &use_stat_update, NULL) != 0, false, "Errore di creazione di use stat thread");
	if(configuration.tui) {CHECKEXIT(pthread_create(&tui_thread, NULL, &print_tui, NULL) != 0, false, "Errore di creazione di use stat thread");}
	int *whoami_list = (int *) malloc(configuration.workers*sizeof(int));
	CHECKALLOC(whoami_list);
	for (size_t i = 0; i < configuration.workers; i++)
		whoami_list[i] = i + 1;
	
	for (int i = 0; i < configuration.workers; i++)
		CHECKEXIT(pthread_create(&workers[i], NULL, &worker, &whoami_list[i]), false, "Errore di creazione dei worker");
	
	if(configuration.tui) print_tui_header(SOCKETADDR);
	while(true){
		SAFELOCK(abort_connections_mtx);
		SAFELOCK(can_accept_mtx);
		if(abort_connections || (!can_accept && clients_active == 0)){
			SAFEUNLOCK(abort_connections_mtx);
			SAFEUNLOCK(can_accept_mtx);
			break;
		} 	
		SAFEUNLOCK(abort_connections_mtx);
		SAFEUNLOCK(can_accept_mtx);	
		poll_val = poll(com_fd, com_count, -1);
		if(poll_val < 0){
			perror("Errore durante la poll!");
			exit(EXIT_FAILURE);
		} 

		if(com_fd[0].revents & POLLIN){
			com = accept(socket_fd, NULL, 0);
			SAFELOCK(can_accept_mtx);
			if(!can_accept){ close(com); }
			SAFEUNLOCK(can_accept_mtx);
			if(com < 0){ CHECKERRNO(com < 0, "Errore durante la accept"); }
			else{
				clients_active++;
				if (clients_active > max_clients_active) max_clients_active = clients_active;
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
			read_bytes = read(good_fd_pipe[0], buffer, sizeof(buffer));
			CHECKERRNO((read_bytes < 0), "Errore durante la lettura della pipe");
			if(strncmp(buffer, "termina", PIPE_BUF) != 0){
				tmp = strtol(buffer, NULL, 10);
				if(tmp <= 0)
					fprintf(stderr, "Errore strtol good_pipe! Buffer -> %s\n", buffer);
					
				else{
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
		SAFELOCK(abort_connections_mtx);
		SAFELOCK(can_accept_mtx);
		if(abort_connections || (!can_accept && clients_active == 0)){
			SAFEUNLOCK(abort_connections_mtx);
			SAFEUNLOCK(can_accept_mtx);
			break;
		} 	
		SAFEUNLOCK(abort_connections_mtx);
		SAFEUNLOCK(can_accept_mtx);	
			
		if(com_fd[2].revents & POLLIN){
			read_bytes = read(done_fd_pipe[0], buffer, sizeof buffer);
			CHECKERRNO((read_bytes < 0), "Errore durante la lettura della pipe");
			tmp = strtol(buffer, NULL, 10);
			if(tmp <= 0)
				fprintf(stderr, "Errore strtol done_pipe! Buffer -> %s\n", buffer);
			else{
				CHECKERRNO(close(tmp) < 0, "Errore chiusura done queue pipe");
				clients_active--;
				if(!clients_active && !can_accept) break;
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
		
	}
	
	SAFELOCK(abort_connections_mtx);
	abort_connections = true;
	SAFEUNLOCK(abort_connections_mtx);
	while(true){
		for (size_t i = 0; i < configuration.workers; i++){
			SAFELOCK(free_threads_mtx);
			if(!free_threads[i]){
				SAFEUNLOCK(free_threads_mtx);
				continue;
			}
			SAFEUNLOCK(free_threads_mtx);
			if(i == configuration.workers - 1)
				goto finish;
		}
	}

finish:
	SAFELOCK(ready_queue_mtx);
	clean_ready_list(&ready_queue[0], &ready_queue[0]);
	for (int i = 0; i < 2*configuration.workers; i++)
		insert_client_list(-2, &ready_queue[0], &ready_queue[1]);
	SAFEUNLOCK(ready_queue_mtx);
	while(true){
		SAFELOCK(ready_queue_mtx);
		pthread_cond_signal(&client_is_ready);
		SAFEUNLOCK(ready_queue_mtx);
		threads_finished = true;
		for(int i = 0; i < configuration.workers; i++){
			SAFELOCK(free_threads_mtx);
			if(free_threads[i]){
				threads_finished = false;
				SAFEUNLOCK(free_threads_mtx);
				break;
			}
			SAFEUNLOCK(free_threads_mtx);
			 
		}
		if(threads_finished) goto join_workers;
	}
join_workers:
	for (int i = 0; i < configuration.workers; i++)
		CHECKEXIT(pthread_join(workers[i], NULL) != 0, false, "Errore durante il join dei workers");
	
	SAFELOCK(server_storage.storage_access_mtx);
	pthread_cond_signal(&start_victim_selector);
	SAFEUNLOCK(server_storage.storage_access_mtx);


	pthread_join(use_stat_thread, NULL);
	for (size_t i = 0; i < com_size; i++){
			if(com_fd[i].fd != 0)
				close(com_fd[i].fd);
	}
	pthread_kill(signal_handler_thread, SIGUSR1);
	pthread_join(signal_handler_thread, NULL);
	if(configuration.tui){
		strcpy(log_buffer, "QUIT");
		CHECKERRNO(write(tui_pipe[1], log_buffer, 20) < 0, "Errore tui pipe");
	}
	sprintf(log_buffer, "Max size reached: %lu", server_storage.max_size_reached);
	write_to_log(log_buffer);
	sprintf(log_buffer, "Max file num reached: %d", server_storage.max_file_num_reached);
	write_to_log(log_buffer);
	sprintf(log_buffer, "Total evictions: %d", server_storage.total_evictions);
	write_to_log(log_buffer);
	sprintf(log_buffer, "Max clients active at the same time: %d", max_clients_active);
	write_to_log(log_buffer);
	close_log();
	close(socket_fd);
	close(good_fd_pipe[0]);
	close(good_fd_pipe[1]);
	close(done_fd_pipe[0]);
	close(done_fd_pipe[1]);
	close(tui_pipe[0]);
	close(tui_pipe[1]);
	print_summary();
	free_config(&configuration);
	clean_storage();
	clean_ready_list(&ready_queue[0], &ready_queue[1]);
	free(workers);
	free(com_fd);
	free(free_threads);
	free(whoami_list);
	pthread_mutex_destroy(&ready_queue_mtx);
	pthread_mutex_destroy(&log_access_mtx);
	pthread_mutex_destroy(&free_threads_mtx);
	pthread_cond_destroy(&client_is_ready);
	// puts("Server closed");
	return 0;
}

char* get_algorithm(unsigned char algo){
	switch (algo){
	case LRU: return "LRU";
	case LFU: return "LFU";
	case LRFU: return "LRFU";
	}
	return "FIFO";
}

void printconf(const char* socketaddr){
	printf(ANSI_COLOR_GREEN CONF_LINE_TOP
		"│ %-12s\t"ANSI_COLOR_YELLOW"%20d"ANSI_COLOR_GREEN" │\n" CONF_LINE
		"│ %-12s\t"ANSI_COLOR_YELLOW"%20d"ANSI_COLOR_GREEN" │\n" CONF_LINE
		"│ %-12s\t"ANSI_COLOR_YELLOW"%20d"ANSI_COLOR_GREEN" │\n" CONF_LINE
		"│ %-12s\t"ANSI_COLOR_YELLOW"%20s"ANSI_COLOR_GREEN" │\n" CONF_LINE
		"│ %-12s\t"ANSI_COLOR_YELLOW"%20s"ANSI_COLOR_GREEN" │\n" CONF_LINE
		"│ %-12s\t"ANSI_COLOR_YELLOW"%20s"ANSI_COLOR_GREEN" │\n" CONF_LINE
		"│ %-12s\t"ANSI_COLOR_YELLOW"%20s"ANSI_COLOR_GREEN" │\n", 
		"Workers:",	configuration.workers, "Max Memory:", configuration.mem, "Max Files:", 
		configuration.files, "Socket file:", basename(socketaddr), "Log:", configuration.log ? configuration.log : "Non disponibile", "Algorithm:", get_algorithm(configuration.replacement_algo), "Compression:", configuration.compression ? "Active" : "Disabled");
	configuration.compression ? printf(CONF_LINE "│ %-12s\t"ANSI_COLOR_YELLOW"%20d"ANSI_COLOR_GREEN" │\n" CONF_LINE_BOTTOM ANSI_COLOR_RESET_N, "Level:", configuration.compression_level) : printf(CONF_LINE_BOTTOM ANSI_COLOR_RESET_N);
}
	
void init(char *sockname, char *config_file){
	FILE *conf = NULL;
	if((conf = fopen(config_file, "r")) == NULL){
		perror("Errore apertura config file");
		exit(EXIT_FAILURE);
	}
	if(parse_config(conf, &configuration) < 0)
		exit(EXIT_FAILURE);
	fclose(conf);
	memset(sockname, 0 , UNIX_MAX_PATH);
	if(configuration.sockname[0] != '/'){
		sprintf(sockname, "/tmp/");
		strncat(sockname, configuration.sockname, AF_UNIX_MAX_PATH-7);
	}
	else
		strncpy(sockname, configuration.sockname, AF_UNIX_MAX_PATH-1);
	
	
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
	CHECKALLOC(com_fd);
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
	CHECKSCEXIT(sigaddset(&sig_set, SIGUSR1), true, "Errore di inizializzazione sig_set");
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
		}
		if(signum == SIGHUP){
			SAFELOCK(can_accept_mtx);
			can_accept = false;
			SAFEUNLOCK(can_accept_mtx);
			sprintf(buffer, "termina");
			CHECKERRNO((write(good_fd_pipe[1], buffer, sizeof(buffer)) < 0), "Errore invio terminazione sulla pipe");
		}
		if(signum == SIGUSR1) break;
	}
	return (void *) 0;
}