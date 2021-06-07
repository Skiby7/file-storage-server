#include "parser.h"
#define MAX_BUFFER_LEN 151

static inline void removeChar(char *token){
	for (int i = strlen(token)-1; i != 0; i--)
		if(token[i] == '\n' || token[i] == '#'){
			token[i] = '\0';
			break;
		}
}

int parseConfig(FILE *conf, config *configuration) {

	char *tmpstr, *token;
	char *buff = (char *) calloc(MAX_BUFFER_LEN, 1);
	int tokenlen = 0;
	while(fgets(buff, MAX_BUFFER_LEN, conf) != NULL){
		token = strtok_r(buff, DELIM, &tmpstr);
		if(token[0] == '\n' || token[0] == '#') continue;

		else if(strcmp(token, "WORKERS") == 0){
			token = strtok_r(NULL, " ", &tmpstr);
			removeChar(token);
			configuration->workers = atoi(token);
		}

		else if(strcmp(token, "MAXMEM") == 0){
			token = strtok_r(NULL, " ", &tmpstr);
			removeChar(token);
			configuration->mem = atoi(token);
		}

		else if(strcmp(token, "MAXFILES") == 0){
			token = strtok_r(NULL, " ", &tmpstr);
			removeChar(token);
			configuration->files = atoi(token);
		}
		
		else if(strcmp(token, "SOCKNAME") == 0){
			token = strtok_r(NULL, " ", &tmpstr);
			
			removeChar(token);
			tokenlen = strlen(token);
			configuration->sockname = (char *) malloc(tokenlen + 1);
			memset(configuration->sockname, 0, tokenlen + 1);
			strncpy(configuration->sockname, token, tokenlen);
		}

		else if(strcmp(token, "LOGFILE") == 0){
			token = strtok_r(NULL, " ", &tmpstr);
			removeChar(token);
			tokenlen = strlen(token);
			configuration->log = (char *) malloc(tokenlen + 1);
			memset(configuration->log, 0, tokenlen + 1);
			strncpy(configuration->log, token, tokenlen);
		}

		else{
			free(buff);
			return -1;
		} 
	}

	
	free(buff);
	return 0;
	
}

void freeConfig(config *configuration){
	free(configuration->sockname);
	free(configuration->log);
}

