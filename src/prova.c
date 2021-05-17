#include "common_includes.h"
#include "server.h"
#include "client_queue.h"
#include "hash.h"
#include "file.h"
#include "fssApi.h"
#include <limits.h>

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


int main(){
	// struct stat a_aout;
	// struct stat M_Makefile;
	
	// int aout;
	// if((aout = open("./a.out", O_RDONLY)) == -1){
	// 	perror("Filein");
	// 	exit(EXIT_FAILURE);
	// }
	// fstat(aout, &a_aout);
	// int Makefile;
	// if((Makefile = open("./Makefile", O_RDONLY)) == -1){
	// 	perror("Makefile");
	// 	exit(EXIT_FAILURE);
	// }
	// fstat(Makefile, &M_Makefile);
	// printf("size: %hho\n", a_aout.st_mode);
	// printf("size: %o\n", M_Makefile.st_mode);

	// unsigned char *buffer = (unsigned char *)calloc(a_aout.st_size, sizeof(unsigned char));
	// read(aout, buffer, a_aout.st_size);
	// int i = 0;
	// // while(i < a_aout.st_size){
	// // 	printf("%x ", buffer[i++]);
	// // }
	// int aoutnew;
	// if((aoutnew = open("./anew.out", O_CREAT | O_RDWR, a_aout.st_mode)) == -1){
	// 	perror("Filein");
	// 	exit(EXIT_FAILURE);
	// }
	// 	write(aoutnew, buffer, a_aout.st_size);
	
	// buffer = realloc(buffer, M_Makefile.st_size);
	// read(Makefile, buffer, M_Makefile.st_size);
	// i = 0;
	// // while(i < M_Makefile.st_size){
	// // 	printf("%c", buffer[i++]);
	// // }
	// puts("");
	// int flags = O_CREATE ;
	// printf("%o\n", flags);
	// int mode = 0;
	// if(flags & O_CREATE)
	// 	mode |= O_CREAT;
	// printf("%o %o\n",O_CREAT, mode);
	// close(aout);
	// close(Makefile);
	// free(buffer);
	CHECKERRNO(openFile("./ciao", 0), "First");
	CHECKERRNO(openFile("ciao", O_CREATE), "Second");
	CHECKERRNO(openFile("anew.out", O_CREATE), "Third");


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