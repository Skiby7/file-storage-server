#include "common_includes.h"
#include "server.h"
#include "client_queue.h"
#include "hash.h"

#include <limits.h>

typedef struct clients_{
	int id;
	struct clients_ *next;
} clients_open;

// void func(clients_list *head){
// 	while (head != NULL){
// 		printf("%d -> ", head->com);
// 		head = head->next;
	
// 	}
// 	puts("NULL");
	
// }

void func1(clients_open *head){
	while (head != NULL){
		printf("%d -> ", head->id);
		head = head->next;
	}
	puts("NULL");
	
}

struct prova
{
	char *ciao;
	int i;
};


static int check_client_id(clients_open *head, int id){
	while(head != NULL){
		if(head->id == id) return -1;
		head = head->next;
	}
	return 0;
}

static int insert_client_open(clients_open **head, int id){
	if(check_client_id((*head), id) == -1) return -1;
	clients_open *new = (clients_open *) malloc(sizeof(clients_open));
	new->id = id;
	new->next = (*head);
	(*head) = new;	
	return 0;
}

static int remove_client_open(clients_open **head, int id){
	clients_open *scanner = (* head);
	clients_open *befree = NULL;
	if((* head)->id == id){
		befree = (* head);
		(* head) = (*head)->next;
		free(befree);
		return 0;
	}
	while(true){
		if(scanner->next == NULL) return -1;
		if(scanner->next->id == id){
			befree = scanner->next;
			scanner->next = scanner->next->next;
			free(befree);
			return 0;
		}
		scanner = scanner->next;
	}
}
int main(){
	
	
	clients_open *ciao = NULL;
	insert_client_open(&ciao, 1);
	insert_client_open(&ciao, 2);
	insert_client_open(&ciao, 1);
	insert_client_open(&ciao, 3);
	printf("%d\n\n", remove_client_open(&ciao, 5));
	insert_client_open(&ciao, 2);
	func1(ciao);
	
	



	return 0;

}

