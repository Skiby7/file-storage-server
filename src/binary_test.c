#include <stdio.h>
#include <stdlib.h>
#include "common_includes.h"
int main(int argc, char* argv[]){
	if(argc != 2){
		fprintf(stderr, "Missing input\n");
		exit(EXIT_FAILURE);
	}
	printf(ANSI_COLOR_MAGENTA"%s:"ANSI_COLOR_CYAN" The quick brown fox jumps over the lazy dog"ANSI_COLOR_RESET_N, argv[1]);
	return 0;
}