#include "client_queue.h"

void insert_client_ready_list(int com, ready_clients **head, ready_clients **tail){
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

int pop_client(ready_clients **head, ready_clients **tail){
	int retval = 0;
	ready_clients *befree = NULL;
	if((*tail) == NULL)
		return -1;
	
	retval = (*tail)->com;
	befree = (*tail);
	if((*tail)->prev != NULL)
		(*tail)->prev->next = NULL;
	
	if(((*tail) = (*tail)->prev) == NULL)
		(*head) = NULL;
	
	free(befree);
	return retval;
	
} 

void clean_list(ready_clients **head){
	ready_clients *befree = NULL;
	while((*head)!=NULL){
		close((*head)->com);
		befree = (*head);
		(*head) = (*head)->next;
		free(befree);

	}
}