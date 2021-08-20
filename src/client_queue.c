#include "client_queue.h"

void insert_client_list(int com, clients_list **head, clients_list **tail){
	clients_list* new = (clients_list*) malloc(sizeof(clients_list));
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

int pop_client(clients_list **head, clients_list **tail){
	int retval = 0;
	clients_list *befree = NULL;
	if((*tail) == NULL)
		return -1;

	retval = (*tail)->com;
	befree = (*tail);
	if((*tail)->prev != NULL)
		(*tail)->prev->next = NULL;
	
	if(((*tail) = (*tail)->prev) == NULL)
		(*head) = NULL;
	
	if(befree){
		free(befree);
		befree = NULL;
	}
	
	return retval;
	
} 

void clean_ready_list(clients_list **head, clients_list **tail){
	clients_list *befree = NULL;
	while((*head)!=NULL){
		close((*head)->com);
		befree = (*head);
		(*head) = (*head)->next;
		free(befree);
		befree = NULL;
	}
	(*tail) = NULL;
}


