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
	char pathname[UNIX_MAX_PATH];
	struct lockers_ *next;
	struct lockers_ *prev;
} lock_waiters_list;



void insert_client_list(int com, clients_list **head, clients_list **tail);
int pop_client(clients_list **head, clients_list **tail);
void clean_ready_list(clients_list **head);
void clean_done_list(clients_list **head, int *client_closed);
void insert_lock_list(int com, int id, char *pathname, lock_waiters_list **head, lock_waiters_list **tail);
int get_lock_client(int *id, char *pathname, lock_waiters_list **head, lock_waiters_list **tail){;