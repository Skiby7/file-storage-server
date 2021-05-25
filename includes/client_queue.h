#ifndef COMMON_INCLUDES_H
#define COMMON_INCLUDES_H
#include "common_includes.h"
#endif

typedef struct connections_{
	int com;
	struct connections_ *next;
	struct connections_ *prev;
} clients_list;

typedef struct lockers_{
	int com;
	int id;
	struct lockers_ *next;
	struct lockers_ *prev;
} lock_waiters;



void insert_client_list(int com, clients_list **head, clients_list **tail);
int pop_client(clients_list **head, clients_list **tail);
void clean_ready_list(clients_list **head);
void clean_done_list(clients_list **head, int *client_closed);
void insert_lock_list(int com, int id, lock_waiters **head, lock_waiters **tail);
int pop_lock_client(int *com, int *id, lock_waiters **head, lock_waiters **tail){;