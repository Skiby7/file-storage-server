#include "common_includes.h"
#include "server.h"
#include "client_queue.h"
#include "hash.h"
#include "file.h"
#include "fssApi.h"
#include <limits.h>

void func(clients_list *head){
	while (head != NULL){
		printf("%d -> ", head->com);
		head = head->next;
	
	}
	puts("NULL");
	
}

void func1(clients_list *tail){
	while (tail != NULL){
		printf("%d -> ", tail->com);
		tail = tail->prev;
	}
	puts("NULL");
	
}

struct prova
{
	char *ciao;
	int i;
};


int main(){
	int pipe_fd[2];
	pipe(pipe_fd);
	struct prova prova_invio;
	prova_invio.ciao = (char *) calloc(10, 1);
	prova_invio.i = 5;
	strcpy(prova_invio.ciao, "Testo");
	struct prova prova_ricezione;
	memset(&prova_ricezione, 0, sizeof(struct prova));
	pid_t pid;
	pid = fork();
	if(pid == 0){
		close(pipe_fd[0]);
		write(pipe_fd[1], &prova_invio, sizeof(prova_invio));
		puts("Inviato!");
		return 0;

	}
	else{
		close(pipe_fd[1]);
		read(pipe_fd[0], &prova_ricezione, sizeof(prova_ricezione));
		printf("Ricevuto:\vStringa: %s\n\tIntero: %d\n", prova_ricezione.ciao, prova_ricezione.i);
		return 0;

	}

	
	



	return 0;

}

