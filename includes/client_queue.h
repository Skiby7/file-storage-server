#ifndef COMMON_INCLUDES_H
#define COMMON_INCLUDES_H
#include "common_includes.h"
#endif

typedef struct connections_{
	int com;
	struct connections_ *next;
	struct connections_ *prev;
} clients_list;




void insert_client_list(int com, clients_list **head, clients_list **tail);
int pop_client(clients_list **head, clients_list **tail);
void clean_ready_list(clients_list **head);
