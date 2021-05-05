#include <clientApi.h>

int openFile(const char* pathname, int flags){
	int filein;
	if((filein = open(pathname, flags)) == -1){
		perror("Filein");
		exit(EXIT_FAILURE);
	}
}



