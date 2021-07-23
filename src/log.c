#include "log.h"

FILE *logfile;
bool log_available;

int open_log(char *pathname){
	logfile = NULL;
	if((logfile = fopen(pathname, "w")) == NULL){
		log_available = false;
		fprintf(stderr, ANSI_COLOR_RED"Impossibile aprire %s! La sessione non verrà registrata."ANSI_COLOR_RESET_N, pathname);
		return -1;
	}
	log_available = true;
	return 0;

}

int write_to_log(char *msg){
	char timestamp[81];
	time_t time_;
	struct tm *lcltime = NULL;
	if(log_available){
		memset(timestamp, 0, 81);
		time(&time_);
		lcltime = localtime(&time_);
		strftime(timestamp, 80, "%d-%m-%Y %X %Z", lcltime);
		fprintf(logfile, "[%s] %s\n", timestamp, msg);
		return 0;
	}
	return -1;
}

int close_log(){
	if(log_available){
		fclose(logfile);
		return 0;	
	}
	return -1;
}