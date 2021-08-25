#include "parser.h"


static void remove_char(char *token){
	for (int i = strlen(token)-1; i != 0; i--)
		if(token[i] == '\n' || token[i] == '#'){
			token[i] = '\0';
			break;
		}
}

static bool isnum(const char *num){
	for (size_t i = 0; i < strlen(num); i++)
		if(!isdigit(num[i])) return false;
	return true;
	
}

int parse_config(FILE *conf, config *configuration) {

	char *tmpstr = NULL, *token = NULL, *tmp = NULL;
	char *buff = (char *) calloc(MAX_BUFFER_LEN, 1);
	int tokenlen = 0;
	bool c_level_set = false;
	while(fgets(buff, MAX_BUFFER_LEN-1, conf) != NULL){
		token = strtok_r(buff, DELIM, &tmpstr);
		if(token[0] == '\n' || token[0] == '#') continue;

		else if(strcmp(token, "WORKERS") == 0){
			token = strtok_r(NULL, " ", &tmpstr);
			remove_char(token);
			if (!isnum(token)){
				fprintf(stderr, ANSI_COLOR_RED"WORKERS non valido, impossibile avviare il server!"ANSI_COLOR_RESET_N); 
				return -1;
			}
			configuration->workers = atoi(token);
			
		}

		else if(strcmp(token, "MAXMEM") == 0){
			token = strtok_r(NULL, " ", &tmpstr);
			remove_char(token);
			if (!isnum(token)){
				fprintf(stderr, ANSI_COLOR_RED"MAXMEM non valido, impossibile avviare il server!"ANSI_COLOR_RESET_N); 
				return -1;
			}
			configuration->mem = atoi(token);
			
		}

		else if(strcmp(token, "MAXFILES") == 0){
			token = strtok_r(NULL, " ", &tmpstr);
			remove_char(token);
			if (!isnum(token)){
				fprintf(stderr, ANSI_COLOR_RED"MAXFILES non valido, impossibile avviare il server!"ANSI_COLOR_RESET_N); 
				return -1;
			}
			configuration->files = atoi(token);
			
		}
		
		else if(strcmp(token, "SOCKNAME") == 0){
			token = strtok_r(NULL, " ", &tmpstr);
			
			remove_char(token);
			tokenlen = strlen(token);
			configuration->sockname = (char *) calloc(tokenlen + 1, sizeof(char));
			strncpy(configuration->sockname, token, tokenlen);
			tmp = (configuration->sockname[0] == '/') ? realpath(dirname(configuration->sockname), NULL) : NULL;
			if (configuration->sockname[0] == '/' && !tmp){
				fprintf(stderr, ANSI_COLOR_RED"Path SOCKNAME non valido, impossibile avviare il server!"ANSI_COLOR_RESET_N); 
				free(tmp);
				return -1;
			}
			free(tmp);
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
			c_level_set = true;
			token = strtok_r(NULL, " ", &tmpstr);
			remove_char(token);
			int level = atoi(token);
			configuration->compression_level = (level >= 0 && level <= 9) ? level : 6;
			if(level < 0 || level > 9) fprintf(stderr, ANSI_COLOR_RED"Il livello di compressione deve essere compreso fra 0 e 9, impostato livello di default 6!"ANSI_COLOR_RESET_N);
		}

		else{
			fprintf(stderr, ANSI_COLOR_RED"Impostazione %s non riconosciuta!"ANSI_COLOR_RESET_N, token);
			free(buff);
			return -1;
		} 
	}

	if (!c_level_set && configuration->compression) configuration->compression_level = 6;
			
	if (!configuration->sockname){
		fprintf(stderr, ANSI_COLOR_RED"SOCKNAME non definito, impossibile avviare il server!"ANSI_COLOR_RESET_N); 
		return -1;
	} 
	if (!configuration->mem){
		fprintf(stderr, ANSI_COLOR_RED"MAXMEM non definito, impossibile avviare il server!"ANSI_COLOR_RESET_N); 
		return -1;
	} 
	if (!configuration->files){
		fprintf(stderr, ANSI_COLOR_RED"MAXFILES non definito, impossibile avviare il server!"ANSI_COLOR_RESET_N); 
		return -1;
	} 
	if (!configuration->workers){
		fprintf(stderr, ANSI_COLOR_RED"WORKERS non definito, impossibile avviare il server!"ANSI_COLOR_RESET_N); 
		return -1;
	} 
	free(buff);
	return 0;
	
}

void free_config(config *configuration){
	free(configuration->sockname);
	free(configuration->log);
}

