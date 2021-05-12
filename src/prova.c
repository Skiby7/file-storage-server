#include "common_includes.h"
#include "server.h"

void func(ready_clients *head){
	while (head != NULL){
		printf("%d -> ", head->com);
		head = head->next;
	
	}
	puts("NULL");
	
}

void func1(ready_clients *tail){
	while (tail != NULL){
		printf("%d -> ", tail->com);
		tail = tail->prev;
	}
	puts("NULL");
	
}

static void clean_list(ready_clients **head){
	ready_clients *befree = NULL;
	while((*head)!=NULL){
		befree = (*head);
		(*head) = (*head)->next;
		free(befree);
	}
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

static void insert_client(int com, ready_clients **head, ready_clients **tail){
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

void printconf(){
	printf(ANSI_COLOR_GREEN CONF_LINE_TOP"│ %-12s\t"ANSI_COLOR_YELLOW"%20d"ANSI_COLOR_GREEN" │\n" CONF_LINE
			"│ %-12s\t"ANSI_COLOR_YELLOW"%20d"ANSI_COLOR_GREEN" │\n" CONF_LINE
			"│ %-12s\t"ANSI_COLOR_YELLOW"%20d"ANSI_COLOR_GREEN" │\n" CONF_LINE
			"│ %-12s\t"ANSI_COLOR_YELLOW"%20s"ANSI_COLOR_GREEN" │\n" CONF_LINE
			"│ %-12s\t"ANSI_COLOR_YELLOW"%20s"ANSI_COLOR_GREEN" │\n" CONF_LINE_BOTTOM"\n"ANSI_COLOR_RESET, "Workers:",
			10, "Mem:", 200, "Files:", 
			3000, "Socket file:", "sockname", "Log:", "logfile");
}

int main(){
	// ready_clients *head = NULL;
	// ready_clients *tail = NULL;
	// for(int i = 0; i < 10000000; i++)
	// 	insert_client(i, &head, &tail);

	
	// func(head);
	// func1(tail);

	// printf(ANSI_COLOR_CYAN"\n%d\n\n"ANSI_COLOR_RESET, pop_client(&tail));

	// func(head);
	// func1(tail);

	// printf(ANSI_COLOR_CYAN"\n%d\n\n"ANSI_COLOR_RESET, pop_client(&tail));

	// func(head);
	// func1(tail);
	// clean_list(&head);

	PRINT_WELCOME;
	printconf();
	return 0;

}
