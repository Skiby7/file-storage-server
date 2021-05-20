#include "common_includes.h"
#include "server.h"
#include "client_queue.h"
#include "hash.h"
#include "file.h"
#include "fssApi.h"
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


int main(){
	struct stat a_aout;
	struct stat M_Makefile;
	
	int photo;
	if((photo = open("./a.out", O_RDONLY)) == -1){
		perror("Filein");
		exit(EXIT_FAILURE);
	}
	fstat(photo, &a_aout);
	

	unsigned char *buffer = (unsigned char *)calloc(a_aout.st_size+10, sizeof(unsigned char));
	read(photo, buffer, a_aout.st_size);
	int i = 0;
	
	int aoutnew1;
	if((aoutnew1 = open("./anew.out", O_CREAT | O_RDWR, a_aout.st_mode)) == -1){
		perror("Filein");
		exit(EXIT_FAILURE);
	}
	write(aoutnew1, buffer, a_aout.st_size);
	while(i < a_aout.st_size+5){
		printf("%c ", buffer[i++]);
	}
	puts("");
	buffer[a_aout.st_size+1] = 'i';
	buffer[a_aout.st_size+2] = 'a';
	buffer[a_aout.st_size+3] = 'o';
	buffer[a_aout.st_size+4] = 'c';
	
	int aoutnew2;
	if((aoutnew2 = open("./anew1.out", O_CREAT | O_RDWR, a_aout.st_mode)) == -1){
		perror("Filein");
		exit(EXIT_FAILURE);
	}
	write(aoutnew2, buffer, a_aout.st_size+4);
	while(i < a_aout.st_size+5){
		printf("%c ", buffer[i++]);
	}
	puts("");
	close(photo);
	close(aoutnew1);
	close(aoutnew2);
	free(buffer);



	return 0;

}

int openFile(const char* pathname, int flags){
	int file;
	int mode = 0;
	int exists = 0;
	int create = O_CREATE && flags;
	if(access(pathname, F_OK) == 0)
		exists = 1;
	
	if(create && exists){
		errno = EEXIST;
		return -1;
	}
	if(!create && !exists){
		errno = EINVAL;
		return -1;
	}
		
	
	if((file = open(pathname, flags)) == -1){
		perror("Open File");
		exit(EXIT_FAILURE);
	}
	return 0;
}