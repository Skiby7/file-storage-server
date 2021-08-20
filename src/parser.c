#include "parser.h"


static void remove_char(char *token){
	for (int i = strlen(token)-1; i != 0; i--)
		if(token[i] == '\n' || token[i] == '#'){
			token[i] = '\0';
			break;
		}
}

int parse_config(FILE *conf, config *configuration) {

	char *tmpstr, *token;
	char *buff = (char *) calloc(MAX_BUFFER_LEN, 1);
	int tokenlen = 0;
	while(fgets(buff, MAX_BUFFER_LEN-1, conf) != NULL){
		token = strtok_r(buff, DELIM, &tmpstr);
		if(token[0] == '\n' || token[0] == '#') continue;

		else if(strcmp(token, "WORKERS") == 0){
			token = strtok_r(NULL, " ", &tmpstr);
			remove_char(token);
			configuration->workers = atoi(token);
		}

		else if(strcmp(token, "MAXMEM") == 0){
			token = strtok_r(NULL, " ", &tmpstr);
			remove_char(token);
			configuration->mem = atoi(token);
		}

		else if(strcmp(token, "MAXFILES") == 0){
			token = strtok_r(NULL, " ", &tmpstr);
			remove_char(token);
			configuration->files = atoi(token);
		}
		
		else if(strcmp(token, "SOCKNAME") == 0){
			token = strtok_r(NULL, " ", &tmpstr);
			
			remove_char(token);
			tokenlen = strlen(token);
			configuration->sockname = (char *) malloc(tokenlen + 1);
			memset(configuration->sockname, 0, tokenlen + 1);
			strncpy(configuration->sockname, token, tokenlen);
		}

		else if(strcmp(token, "LOGFILE") == 0){
			token = strtok_r(NULL, " ", &tmpstr);
			remove_char(token);
			tokenlen = strlen(token);
			configuration->log = (char *) malloc(tokenlen + 1);
			memset(configuration->log, 0, tokenlen + 1);
			strncpy(configuration->log, token, tokenlen);
		}

		else if(strcmp(token, "TUI") == 0){
			token = strtok_r(NULL, " ", &tmpstr);
			remove_char(token);
			configuration->tui = (token[0] == 'y') ? true : false;
		}

		else if(strcmp(token, "COMPRESSION") == 0){
			token = strtok_r(NULL, " ", &tmpstr);
			remove_char(token);
			configuration->compression = (token[0] == 'y') ? true : false;
		}

		else if(strcmp(token, "C_LEVEL") == 0){
			token = strtok_r(NULL, " ", &tmpstr);
			remove_char(token);
			int level = atoi(token);
			configuration->compression_level = (level >= 0 && level <= 9) ? level : 6;
			if(level < 0 || level > 9) fprintf(stderr, ANSI_COLOR_RED"Il livello di compressione deve essere compreso fra 0 e 9, impostato livello di default 6!"ANSI_COLOR_RESET_N);
		}

		else{
			free(buff);
			return -1;
		} 
	}

	
	free(buff);
	return 0;
	
}

void free_config(config *configuration){
	free(configuration->sockname);
	free(configuration->log);
}

