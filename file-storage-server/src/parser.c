#include "parser.h"

char *strtok_r(char *str, const char *delim, char **saveptr);

int parseConfig(FILE *conf, config *configuration) {
	char *tmpstr, *token;
	char *buff = (char *) malloc(MAX_BUFFER_LEN);
	int tokenlen = 0;
	memset(buff, 0, MAX_BUFFER_LEN);
	while(fgets(buff, MAX_BUFFER_LEN, conf) != NULL){
		token = strtok_r(buff, DELIM, &tmpstr);
		if(strcmp(token, "WORKERS") == 0){
			token = strtok_r(NULL, " ", &tmpstr);
			token[strcspn(token, "\n")] = 0;
			configuration->workers = atoi(token);
		}
		else if(strcmp(token, "MEM") == 0){
			token = strtok_r(NULL, " ", &tmpstr);
			token[strcspn(token, "\n")] = 0;
			configuration->mem = atoi(token);
		}
		else if(strcmp(token, "SOCKNAME") == 0){
			token = strtok_r(NULL, " ", &tmpstr);
			token[strcspn(token, "\n")] = 0;
			tokenlen = strlen(token);
			configuration->sockname = (char *) malloc(tokenlen + 1);
			memset(configuration->sockname, 0, tokenlen + 1);
			strncpy(configuration->sockname, token, tokenlen);
		}
		else{
			free(buff);
			return -1;
		} 
	}

	
	free(buff);
	return 0;
	
}

