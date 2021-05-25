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
	
	free(befree);
	return retval;
	
} 

void insert_lock_list(int com, int id, lock_waiters **head, lock_waiters **tail){
	lock_waiters* new = (lock_waiters*) malloc(sizeof(lock_waiters));
	CHECKALLOC(new, "Errore inserimento nella lista lock");
	new->com = com;
	new->id = id;
	new->next = (*head);
	new->prev = NULL;
	if((*tail) == NULL)
		(*tail) = new;
	if((*head) != NULL)
		(*head)->prev = new;
	(*head) = new;	
} 

int pop_lock_client(int *com, int *id, lock_waiters **head, lock_waiters **tail){
	lock_waiters *befree = NULL;
	if((*tail) == NULL)
		return -1;

	*com = (*tail)->com;
	*id = (*tail)->id;
	befree = (*tail);
	if((*tail)->prev != NULL)
		(*tail)->prev->next = NULL;
	
	if(((*tail) = (*tail)->prev) == NULL)
		(*head) = NULL;
	
	free(befree);
	return 0;
	
}

void clean_ready_list(clients_list **head){
	clients_list *befree = NULL;
	while((*head)!=NULL){
		close((*head)->com);
		befree = (*head);
		(*head) = (*head)->next;
		free(befree);
	}
}

void clean_done_list(clients_list **head, int *client_closed){
	clients_list *befree = NULL;
	while((*head)!=NULL){
		close((*head)->com);
		befree = (*head);
		(*head) = (*head)->next;
		free(befree);
		*client_closed+=1;
	}
}

