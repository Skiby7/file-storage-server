#include "server.h"
#include "parser.h"
#include "file.h"
#include "client_queue.h"
#include "log.h"
#define DEFAULTFDS 10



config configuration; // Server config
volatile sig_atomic_t can_accept = 1;
volatile sig_atomic_t abort_connections = 0;
pthread_mutex_t ready_queue_mtx = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t log_access_mtx = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t client_is_ready = PTHREAD_COND_INITIALIZER;

bool *free_threads;
clients_list *ready_queue[2];

int good_fd_pipe[2]; // 1 lettura, 0 scrittura
int done_fd_pipe[2]; // 1 lettura, 0 scrittura
extern void* worker(void* args);
pthread_mutex_t free_threads_mtx = PTHREAD_MUTEX_INITIALIZER;

extern storage server_storage;
extern pthread_cond_t start_victim_selector;


void signal_handler(int signum) {
    if (signum == SIGHUP) {
        can_accept = 0;
    }
    else{
		can_accept = 0;
		abort_connections = 1;
	}
}
        


void func(clients_list *head){
	while (head != NULL){
		printf("%d -> ", head->com);
		head = head->next;
	
	}
	puts("NULL");
	
}

void print_textual_ui(char* SOCKETADDR){
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
	
	int socket_fd = 0, com = 0,  read_bytes = 0, tmp = 0, poll_val = 0, clients_active = 0, max_clients_active = 0; // i = 0, ready_com = 0
	char buffer[PIPE_BUF]; // Buffer per inviare messaggi sullo stato dell'accettazione al client
	char SOCKETADDR[AF_UNIX_MAX_PATH]; // Indirizzo del socket
	char log_buffer[200] = {0};
	struct pollfd *com_fd =  (struct pollfd *) malloc(DEFAULTFDS*sizeof(struct pollfd));
	nfds_t com_count = 0;
	nfds_t com_size = DEFAULTFDS;
	ready_queue[0] = NULL;
	ready_queue[1] = NULL;


	CHECKALLOC(com_fd, "pollfd");
	bool thread_finished = false;
	pthread_t *workers;
	pthread_t use_stat_thread;
	struct sigaction sig_handler;
	memset(&sig_handler, 0, sizeof sig_handler);
	sig_handler.sa_handler = signal_handler;
	sigset_t signal_mask;
	struct sockaddr_un sockaddress; // Socket init
	sigemptyset(&signal_mask);
    sigaddset(&signal_mask, SIGINT);
    sigaddset(&signal_mask, SIGHUP);
    sigaddset(&signal_mask, SIGQUIT);
    sig_handler.sa_mask = signal_mask;
	sigaction(SIGINT, &sig_handler, NULL);
    sigaction(SIGHUP, &sig_handler, NULL);
    sigaction(SIGQUIT, &sig_handler, NULL);

	init(SOCKETADDR, argv[1]); // Configuration struct is now initialized
	open_log(configuration.log);

	write_to_log("Segnali mascherati.");

	init_table(configuration.files, configuration.mem, configuration.compression, configuration.compression_level);
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
	// CHECKEXIT(pthread_create(&signal_handler_thread, NULL, &sig_wait_thread, NULL) != 0, false, "Errore di creazione del signal handler thread");
	CHECKEXIT(pthread_create(&use_stat_thread, NULL, &use_stat_update, NULL) != 0, false, "Errore di creazione di use stat thread");
	for (int i = 0; i < configuration.workers; i++){
		CHECKEXIT(pthread_create(&workers[i], NULL, &worker, &i), false, "Errore di creazione dei worker");
	}
	if(configuration.tui) print_textual_ui(SOCKETADDR);
	while(!abort_connections){
		poll_val = poll(com_fd, com_count, -1);
		if(poll_val < 0){
			if(errno == EINTR){
				if(abort_connections || (!can_accept && clients_active == 0)) break;
				continue;
			}
			perror("Errore durante la poll!");
			exit(EXIT_FAILURE);
		} 
		// PRINT_POLLING(poll_print);
		// printf("poll_val -> %d\n", poll_val);
		// SAFELOCK(abort_connections_mtx);
		// if(abort_connections){
		// 	SAFEUNLOCK(abort_connections_mtx);
		// 	break;
		// }
		// SAFEUNLOCK(abort_connections_mtx);
		if(com_fd[0].revents & POLLIN){
			com = accept(socket_fd, NULL, 0);
			if(!can_accept) close(com);
			else if(com < 0){ CHECKERRNO(com < 0, "Errore durante la accept"); }
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
		
		if(!can_accept && !clients_active){
			while (!thread_finished){
				for (size_t i = 0; i < configuration.workers; i++){
					puts("waiting");
					SAFELOCK(free_threads_mtx);
					if (!free_threads[i]){
						SAFEUNLOCK(free_threads_mtx);
						break;
					}
					if (free_threads[i] && i == configuration.workers - 1)
						thread_finished = true;
					SAFEUNLOCK(free_threads_mtx);
				}
			}
			break;
		}
	}
	
	
	
	SAFELOCK(ready_queue_mtx);
	clean_ready_list(&ready_queue[0], &ready_queue[0]);
	for (int i = 0; i < configuration.workers; i++)
		insert_client_list(-2, &ready_queue[0], &ready_queue[1]);

	pthread_cond_broadcast(&client_is_ready); // sveglio tutti i thread
	SAFEUNLOCK(ready_queue_mtx);
	// for (int i = 0; i < configuration.workers; i++)
	// 	pthread_cancel(workers[i]);
		

	
	for (int i = 0; i < configuration.workers; i++)
		CHECKEXIT(pthread_join(workers[i], NULL) != 0, false, "Errore durante il join dei workers");
	pthread_cancel(use_stat_thread);
	
	CHECKEXIT(pthread_join(use_stat_thread, NULL) != 0, false, "Errore durante la cancellazione dei workers attivi");
	for (size_t i = 0; i < com_size; i++){
			if(com_fd[i].fd != 0)
				close(com_fd[i].fd);
	}
	sprintf(log_buffer, "Max size reached: %lu", server_storage.max_size_reached);
	write_to_log(log_buffer);
	sprintf(log_buffer, "Max file num reached: %d", server_storage.max_file_num_reached);
	write_to_log(log_buffer);
	sprintf(log_buffer, "Total evictions: %d", server_storage.total_evictions);
	write_to_log(log_buffer);
	sprintf(log_buffer, "Max clients active at same time: %d", max_clients_active);
	write_to_log(log_buffer);
	close_log();
	close(socket_fd);
	close(good_fd_pipe[0]);
	close(good_fd_pipe[1]);
	close(done_fd_pipe[0]);
	close(done_fd_pipe[1]);
	print_summary();
	freeConfig(&configuration);
	clean_storage();
	clean_ready_list(&ready_queue[0], &ready_queue[1]);
	free(workers);
	free(com_fd);
	free(free_threads);
	pthread_mutex_destroy(&ready_queue_mtx);
	pthread_mutex_destroy(&log_access_mtx);
	pthread_mutex_destroy(&free_threads_mtx);
	pthread_cond_destroy(&client_is_ready);
	puts("Server closed");
	return 0;
}

void printconf(const char* socketaddr){

	printf(ANSI_COLOR_GREEN CONF_LINE_TOP
		"│ %-12s\t"ANSI_COLOR_YELLOW"%20d"ANSI_COLOR_GREEN" │\n" CONF_LINE
		"│ %-12s\t"ANSI_COLOR_YELLOW"%20d"ANSI_COLOR_GREEN" │\n" CONF_LINE
		"│ %-12s\t"ANSI_COLOR_YELLOW"%20d"ANSI_COLOR_GREEN" │\n" CONF_LINE
		"│ %-12s\t"ANSI_COLOR_YELLOW"%20s"ANSI_COLOR_GREEN" │\n" CONF_LINE
		"│ %-12s\t"ANSI_COLOR_YELLOW"%20s"ANSI_COLOR_GREEN" │\n" CONF_LINE
		"│ %-12s\t"ANSI_COLOR_YELLOW"%20s"ANSI_COLOR_GREEN" │\n", 
		"Workers:",	configuration.workers, "Max Memory:", configuration.mem, "Max Files:", 
		configuration.files, "Socket file:", socketaddr, "Log:", configuration.log, "Compression:", configuration.compression ? "Active" : "Disabled");
	configuration.compression ? printf(CONF_LINE "│ %-12s\t"ANSI_COLOR_YELLOW"%20d"ANSI_COLOR_GREEN" │\n" CONF_LINE_BOTTOM ANSI_COLOR_RESET_N, "Level:", configuration.compression_level) : printf(CONF_LINE_BOTTOM ANSI_COLOR_RESET_N);

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

