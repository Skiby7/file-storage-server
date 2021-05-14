#ifndef COMMON_INCLUDES_H
#define COMMON_INCLUDES_H
#include "common_includes.h"
#endif

typedef struct connections_{
	int com;
	struct connections_ *next;
	struct connections_ *prev;
} ready_clients;

void insert_client_ready_list(int com, ready_clients **head, ready_clients **tail);
int pop_client(ready_clients **head, ready_clients **tail);
void clean_list(ready_clients **head);