#include "server.h"
#include "parser.h"
#include "file.h"
#define _GNU_SOURCE 
#define DEFAULTFDS 100

config configuration; // Server config
volatile sig_atomic_t can_accept = true;
volatile sig_atomic_t abort_connections = false;
pthread_mutex_t targs_mtx = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t targs_read_cond = PTHREAD_COND_INITIALIZER;
pthread_cond_t pollfd_access = PTHREAD_COND_INITIALIZER;
bool targs_read = false;

unsigned int *active_connecitons;


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
	
	int socket_fd = 0, com = 0, i = 0, read_bytes = 0, tmp = 0, poll_val = 0;
	int m_w_pipe[2]; // 0 lettura, 1 scrittura
	char buff[20]; // Buffer per inviare messaggi sullo stato dell'accettazione al client
	char SOCKETADDR[UNIX_MAX_PATH]; // Indirizzo del socket
	struct pollfd *com_fd =  (struct pollfd *) malloc(DEFAULTFDS*sizeof(struct pollfd));
	nfds_t com_count = 0;
	nfds_t com_size = 0;
	CHECKALLOC(com_fd, "pollfd");
	pthread_t waiter; // Thread che molto probabilmente non serviranno
	pthread_t refuser;
	struct sockaddr_un sockaddress; // Socket init
	pargs targs; // Argomenti da passare al thread che molto probabilmente non serviranno
	unsigned int seed = time(NULL);
	ready_clients *ready_clients_head = NULL;
	ready_clients *ready_clients_tail = NULL;
	
	// Signal handler
	struct sigaction sig; 
	memset(&sig, 0, sizeof(sig));
	sig.sa_handler=signal_handler;
	sigaction(SIGINT,&sig,NULL);
	sigaction(SIGHUP,&sig,NULL);
	sigaction(SIGQUIT,&sig,NULL);
	// END signal handler


	init(SOCKETADDR); // Configuration struct is now initialized

	pthread_t *workers = (pthread_t *) malloc(configuration.workers*sizeof(pthread_t)); // Pool di workers
	CHECKALLOC(workers, "workers array");
	active_connecitons = (unsigned int *) malloc(configuration.workers*sizeof(unsigned int));
	memset(com_fd, -1, sizeof(struct pollfd));
	memset(workers, 0, configuration.workers*sizeof(pthread_t));
	memset(active_connecitons, 0, configuration.workers*sizeof(int));
	memset(&targs, 0, sizeof(pargs));
	memset(buff, 0, 10);
	CHECKEXIT((pipe(m_w_pipe) == -1), true, "Impossibile inizializzare la pipe");
	printconf();
	return 0;
	strncpy(sockaddress.sun_path, SOCKETADDR, UNIX_MAX_PATH);
	
	sockaddress.sun_family = AF_UNIX;
	socket_fd = socket(AF_UNIX, SOCK_STREAM, 0);
	unlink(SOCKETADDR);
	CHECKSCEXIT(bind(socket_fd, (struct sockaddr *) &sockaddress, sizeof(sockaddress)), true, "Non sono riuscito a fare la bind");
	CHECKSCEXIT(listen(socket_fd, 10), true, "Impossibile effettuare la listen");
	com_fd[0].fd = socket_fd;
	com_fd[0].events = POLLIN;
	com_fd[1].fd = m_w_pipe[0];
	com_fd[1].events = POLLIN;
	com_count = 2;
	com_size = com_count;
	while(true){

		if(can_accept){
	restart_polling:		
			poll_val = poll(com_fd, com_count, 100);
			CHECKSCEXIT(poll_val, true, "Error while polling");
			if(poll_val == 0){
				// Check if a thread is not busy and assign work
			}
			if(com_fd[0].revents & POLLIN){
				com = accept(socket_fd, NULL, 0);
				CHECKERRNO((com < 0), "Errore durante la accept");
				pthread_mutex_lock(&pollfd_access);
				insert_com_fd(com, &com_size, &com_count, com_fd);
				pthread_mutex_unlock(&pollfd_access);
			}
			if(com_fd[1].revents & POLLIN){
				read_bytes = read(m_w_pipe[0], buff, sizeof(buff));
				CHECKERRNO((read_bytes < 0), "Errore durante la lettura della pipe");
				tmp = atoi(buff);
				if(tmp < 0){
					fprintf(stderr, "Errore atoi! Buffer -> %s\n", buff);
					fflush(stderr);
					continue;
				}
				pthread_mutex_lock(&pollfd_access);
				insert_com_fd(tmp, &com_size, &com_count, com_fd);
				pthread_mutex_unlock(&pollfd_access);
			}
			for(int i = 2; i < com_size; i++){
				if(com_fd[i].fd == -1) continue;

				if(com_fd[i].revents & POLLIN){
					insert_client_ready_list(com_fd[i].fd, &ready_clients_head, &ready_clients_tail);
					com_fd[i].fd = -1;
					com_fd[i].events = 0;
					com_count--;
				}
					
			}

			
			com = accept(socket_fd, NULL, 0);
			CHECKERRNO((com < 0), "Errore durante la accept");
			sprintf(buff, "accepted");
			write(com, buff, strlen(buff));
			memset(buff, 0, sizeof(buff));
			i = rand_r(&seed)%configuration.workers;
			targs.socket_fd = com;
			targs.whoami = i;
			pthread_create(&workers[i], NULL, connection_handler, (void *) &targs);
			sleep(2);
			pthread_mutex_lock(&targs_mtx);
			while(!targs_read) pthread_cond_wait(&targs_read_cond, &targs_mtx);
			targs_read = false;
			pthread_mutex_unlock(&targs_mtx);
			
		}
		else{
			
			pthread_create(&waiter, NULL, wait_workers, &workers);
			pthread_create(&refuser, NULL, refuse_connection, &socket_fd);
			
			pthread_detach(refuser);
			
			puts(ANSI_COLOR_BLUE"Tutti i client si sono disconnesi e sono uscito dal dispatcher"ANSI_COLOR_RESET);
						
			break;
			
		}
		
	}
	pthread_join(waiter, NULL);
	close(socket_fd);
	clean_list(&ready_clients_head);
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




	
	

void printconf(){
	printf("Workers: %d\n"
			"Mem: %d\n"
			"Files: %d\n"
			"Sockname: %s\n"
			"Log: %s\n", 
			configuration.workers, configuration.mem, 
			configuration.files, configuration.sockname, configuration.log);
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

static void insert_com_fd(int com, nfds_t *size, nfds_t *count, struct pollfd *com_fd){
	int free_slot = 0;
	while(free_slot < *size && com_fd[free_slot].fd != -1)
		free_slot++;
	if(free_slot == *size)
		*size = realloc_com_fd(com_fd, free_slot);
	com_fd[free_slot].fd = com;
	com_fd[free_slot].events = POLLIN;
	*count++;
}

static nfds_t realloc_com_fd(struct pollfd *com_fd, nfds_t free_slot){

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

static void insert_client_ready_list(int com, ready_clients **head, ready_clients **tail){
	ready_clients* new = (ready_clients*) malloc(sizeof(ready_clients));
	CHECKALLOC(new, "Errore inserimento nella lista pronti");
	new->com = com;
	new->next = (*head);
	new->prev = NULL;
	if((*tail) == NULL)
		(*tail) = new;
	if((*head) != NULL)
		(*head)->prev = new;
	(*head) = new;	
} 

static int pop_client(ready_clients **tail){
	int retval = 0;
	ready_clients *befree = NULL;
	if((*tail) == NULL)
		return -1;
	retval = (*tail)->com;
	befree = (*tail);
	(*tail)->prev->next = NULL;
	(*tail) = (*tail)->prev;
	free(befree);
	return(retval);
	
} 

static void clean_list(ready_clients **head){
	ready_clients *befree = NULL;
	while((*head)!=NULL){
		befree = (*head);
		(*head) = (*head)->next;
		free(befree);
	}
}