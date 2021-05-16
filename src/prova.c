#include "common_includes.h"
#include "server.h"
#include "client_queue.h"
#include "file.h"

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







unsigned int hash_pjw(void* key){
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
void printconf(){
	printf(ANSI_COLOR_GREEN CONF_LINE_TOP"│ %-12s\t"ANSI_COLOR_YELLOW"%20d"ANSI_COLOR_GREEN" │\n" CONF_LINE
			"│ %-12s\t"ANSI_COLOR_YELLOW"%20d"ANSI_COLOR_GREEN" │\n" CONF_LINE
			"│ %-12s\t"ANSI_COLOR_YELLOW"%20d"ANSI_COLOR_GREEN" │\n" CONF_LINE
			"│ %-12s\t"ANSI_COLOR_YELLOW"%20s"ANSI_COLOR_GREEN" │\n" CONF_LINE
			"│ %-12s\t"ANSI_COLOR_YELLOW"%20s"ANSI_COLOR_GREEN" │\n" CONF_LINE_BOTTOM"\n"ANSI_COLOR_RESET, "Workers:",
			10, "Mem:", 200, "Files:", 
			3000, "Socket file:", "sockname", "Log:", "logfile");
}

static inline unsigned int hash_scan(void* key, unsigned int i, unsigned int max_len){
	return ((hash_pjw(key) + (i/4) + ((i*i)/7))%max_len);

}

unsigned int search_file(char* pathname){
	puts(pathname);
	int i = 0, max_len = 1453;
	while(i < 10) {
		printf("Hash scan: %d\n", hash_scan(pathname, i, max_len));
		i++;
	}
	return 0;
	
}

int main(){


	// PRINT_WELCOME;
	// printconf();
	// ready_clients *client_queue[] = {NULL, NULL};
	// int prova = 100;
	// char buff[10];
	// for(int i = 0; i < 10; i++)
	// 	insert_client_ready_list(i, &client_queue[0], &client_queue[1]);

	// for(int i = 0; i < 10; i++){
	// 	func(client_queue[0]);
	// 	func1(client_queue[1]);
	// 	printf(ANSI_COLOR_CYAN"\n%d\n\n"ANSI_COLOR_RESET, pop_client(&client_queue[0], &client_queue[1]));

	// }
	// func(client_queue[0]);
	// func1(client_queue[1]);
	// insert_client_ready_list(2, &client_queue[0], &client_queue[1]);
	// clean_list(&client_queue);
	// sprintf(buff, "%d", prova);
	// puts(buff);

	search_file("/tmp/sockfile.sk");
	search_file("/tmp/sockfile.s");
	search_file("/tmp/sockfilesk");
	search_file("/tmp/hello.txt");
	search_file("/tmp/hallo.txt");
	search_file("/");



	return 0;

}
