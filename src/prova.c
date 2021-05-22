#include "common_includes.h"
#include "server.h"
#include "client_queue.h"
#include "hash.h"
#include "file.h"
#include "fssApi.h"
#include "connections.h"
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
	int file_not_exists = 0x00;
	
	int file_exists = 0xff;
	int flags = O_LOCK ;
	
	
	printf("%hhx\n", flags & O_CREATE);
	

	
	



	return 0;

}

