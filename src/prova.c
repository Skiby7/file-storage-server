#include "common_includes.h"
#include "server.h"
#include "client_queue.h"


#include <limits.h>


#define BITS_IN_int     ( sizeof(int) * CHAR_BIT )
#define THREE_QUARTERS  ((int) ((BITS_IN_int * 3) / 4))
#define ONE_EIGHTH      ((int) (BITS_IN_int / 8))
#define HIGH_BITS       ( ~((unsigned int)(~0) >> ONE_EIGHTH ))

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


void ulong_to_char(unsigned long integer, unsigned char **array){
	int n = sizeof(unsigned long);
	unsigned char temp[n];
	memset(temp, 0, n);
	*array = (unsigned char *)calloc(n, sizeof(unsigned char));
	n *= 8;
	n -= 8;

	for(int i = 0; i < sizeof(unsigned long); i++, n -= 8)
    	temp[i] = (integer >> n) & 0xff;
	memcpy(*array, temp, sizeof(unsigned long));
}


void uint_to_char(unsigned int integer, unsigned char **array){
	int n = sizeof(unsigned int);
	unsigned char temp[n];
	memset(temp, 0, n);
	*array = (unsigned char *)calloc(n, sizeof(unsigned char));
	n *= 8;
	n -= 8;
	for(int i = 0; i < sizeof(unsigned int); i++, n -= 8)
		temp[i] = (integer >> n) & 0xff;
    	
	memcpy(*array, temp, sizeof(unsigned int));
}

unsigned int char_to_uint(unsigned char *array){
	int n = sizeof(unsigned int) * 8;
	n -= 8;
	unsigned int integer = 0;
	for(int i = 0; i < sizeof(unsigned int); i++, n -= 8)
		integer |= (array[i] << n);
	
	
	return integer;	 
}

unsigned long char_to_ulong(unsigned char *array){
	int n = sizeof(unsigned long) * 8;
	n -= 8;
	unsigned long long_ = 0;
	for(int i = 0; i < sizeof(unsigned long); i++, n -= 8)
		long_ |= (array[i] << n);

	return long_;
}

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
int allocoescrivo(char **buffer){
	*buffer = (char *) malloc(10);

	strcpy(*buffer, "ciao");
	return 0;
}

static inline unsigned int hash_pjw(const void* key){
    char *datum = (char *)key;
    unsigned int hash_value, i;

    if(!datum) return 0;

    for (hash_value = 0; *datum; ++datum) {
        hash_value = (hash_value << ONE_EIGHTH) + *datum;
        if ((i = hash_value & HIGH_BITS) != 0)
            hash_value = (hash_value ^ (i >> THREE_QUARTERS)) & ~HIGH_BITS;
    }
    return (hash_value);
}

static inline unsigned int fnv_hash_function(const void *key, int len) {
    unsigned char *p = (unsigned char*) key;
    unsigned int h = 2166136261u;
    int i;
    for ( i = 0; i < len; i++ )
        h = ( h * 16777619 ) ^ p[i];
    return h;
}

static unsigned int hash_val(const void* key, unsigned int i, unsigned int max_len, unsigned int key_len){
	return ((hash_pjw(key) + i*fnv_hash_function(key, key_len)) % max_len);
}

bool get_ack(int com){
	unsigned char acknowledge = com;
	// if(safe_read(com, &acknowledge, 1) < 0) return -1;
	return (acknowledge == 0x01) ? true : false;
}
int main(){
	// char path[] = "README.md";
	// printf("%d\n", hash_val(path, 0, 100, sizeof(path)));
	// char *string = NULL;
	// string = malloc(10);
	// if(!string){
	// 	puts("string is null");
	// }

	// if(string){
	// 	puts("string is not null");
	// }
	// free(string);
	// char *temp = NULL;
	// unsigned long i = 294;
	// ulong_to_char(i, &temp);
	// printf("%lu\n", char_to_ulong(temp));
	// free(temp);
	printf("Ack %d\n", get_ack(0x01));
	printf("Ack %d\n", get_ack(0x00));



	return 0;

}

