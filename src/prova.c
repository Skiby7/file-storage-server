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
	unsigned char op1 = O_LOCK | READ | O_CREATE | APPEND;
	unsigned char op2 = op1 & O_CREATE;
	unsigned char op3 = op1 & ~WRITE;

	
	printf("Condizione: %d %.8x\n", op1, op1);
	printf("Condizione: %d %.8x\n", op2, op2);
	printf("Condizione: %d %.8x\n", op3, op3);

	if(op1 & WRITE & APPEND)
		puts("ciao\n");

	
	



	return 0;

}

