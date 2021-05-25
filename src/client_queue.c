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

void insert_lock_list(int com, int id, char *pathname, lock_waiters_list **head, lock_waiters_list **tail){
	lock_waiters_list* new = (lock_waiters_list*) malloc(sizeof(lock_waiters_list));
	CHECKALLOC(new, "Errore inserimento nella lista lock");
	new->com = com;
	new->id = id;
	strncpy(new->pathname, pathname, UNIX_MAX_PATH);
	new->next = (*head);
	new->prev = NULL;
	if((*tail) == NULL)
		(*tail) = new;
	if((*head) != NULL)
		(*head)->prev = new;
	(*head) = new;	
} 

int get_lock_client(int *id, char *pathname, lock_waiters_list **head, lock_waiters_list **tail){
	lock_waiters_list *befree = NULL;
	lock_waiters_list *scanner = (*tail);
	int retval = 0;
	if((*tail) == NULL)
		return -1;

	while(scanner->prev != NULL && strncmp(scanner->pathname, pathname, UNIX_MAX_PATH) == 0)
		scanner = scanner->prev;

	if(scanner == NULL) return -1;
	befree = scanner;
	retval = scanner->com;
	*id = scanner->com;
	if(scanner->prev != NULL)
		scanner->prev->next = scanner->next;
	else
		(*head) = scanner->next;

	if(scanner->next != NULL)
		scanner->next->prev = scanner->prev;
	else	
		(*tail) = scanner->prev;
	
	free(befree);
	return retval;
	
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

