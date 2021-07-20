#include "serialization.h"

void ulong_to_char(unsigned long integer, unsigned char *array){
	int n = sizeof(unsigned long);
	memset(array, 0, n);
	n *= 8;
	n -= 8;
	for(int i = 0; i < sizeof(unsigned long); i++, n -= 8)
		array[i] = (integer >> n) & 0xff;
}


void uint_to_char(unsigned int integer, unsigned char *array){
	int n = sizeof(unsigned int);
	memset(array, 0, n);
	n *= 8;
	n -= 8;
	for(int i = 0; i < sizeof(unsigned int); i++, n -= 8)
		array[i] = (integer >> n) & 0xff;
}

unsigned int char_to_uint(unsigned char *array){
	int n = sizeof(unsigned int) * 8;
	n -= 8;
	unsigned int integer = 0;
	for(int i = 0; i < sizeof(unsigned int); i++, n -= 8)
		integer |= (array[i] << n);
	
	return integer;	 
}

unsigned long char_to_ulong(unsigned char *array){
	int n = sizeof(unsigned long) * 8;
	n -= 8;
	unsigned long long_ = 0;
	for(int i = 0; i < sizeof(unsigned long); i++, n -= 8)
		long_ |= (array[i] << n);

	return long_;
}

int serialize_request(client_request request, unsigned char** buffer, unsigned long* buffer_len){
	int increment = 0;
	unsigned char tmp_int[sizeof(unsigned int)];
	unsigned char tmp_long[sizeof(unsigned long)];
	// printf("client: %u\ncommand: 0x%.2x\nflags: 0x%.2x\npath: %s\nsize: %lu\n", request.client_id,request.command,request.flags,request.pathname,request.size);
	*buffer_len = sizeof(request.client_id) + sizeof(request.command) + sizeof(request.flags) + sizeof(request.files_to_read) + sizeof(request.pathlen) + request.pathlen + sizeof(request.size) + request.size;
	(*buffer) = (unsigned char *) calloc(*buffer_len, sizeof(unsigned char));

	uint_to_char(request.client_id, tmp_int);
	memcpy(*buffer, tmp_int, sizeof(request.client_id));
	increment += sizeof(request.client_id);

	memcpy(*buffer + increment, &request.command, sizeof(request.command));
	increment += sizeof(request.command);

	memcpy(*buffer + increment, &request.flags, sizeof(request.flags));
	increment += sizeof(request.flags);

	if(request.files_to_read <= 0) memset(tmp_int, 0xff, sizeof tmp_int);
	else uint_to_char((unsigned int)request.files_to_read, tmp_int);
	memcpy(*buffer + increment, tmp_int, sizeof(request.files_to_read));
	increment += sizeof(request.files_to_read);

	uint_to_char(request.pathlen, tmp_int);

	memcpy(*buffer + increment, tmp_int, sizeof(request.pathlen)); // request.pathlen = strlen(request.pathname) + 1
	increment += sizeof(request.pathlen);
	
	if(request.pathlen){
		memcpy(*buffer + increment, request.pathname, request.pathlen);
		increment += request.pathlen;
	}
	

	ulong_to_char(request.size, tmp_long);
	memcpy(*buffer + increment, tmp_long, sizeof(request.size));
	increment += sizeof(request.size);

	if(request.size) memcpy(*buffer + increment, request.data, request.size);
	// printf("client: %u\ncommand: 0x%.2x\nflags: 0x%.2x\npath: %s\nsize: %lu\n", request.client_id,request.command,request.flags,request.pathname,request.size);
	
	
	return 0;
}

int deserialize_request(client_request *request, unsigned char** buffer, unsigned long buffer_len){

	// int increment = sizeof(unsigned long);
	int increment = 0;
	unsigned char tmp_int[sizeof(unsigned int)];
	unsigned char tmp_long[sizeof(unsigned long)];
	// if(buffer_len > 10000000){
	// 	for (size_t i = 0; i < buffer_len - 10000000; i++){
	// 		printf("%.2x ", (*buffer)[i]);
	// 	}
	// 	puts("");
	// 	exit(0);
	// }
		
	memset(tmp_int, 0, sizeof(unsigned int));
	
	memcpy(tmp_int, *buffer,  sizeof(request->client_id));
	request->client_id = char_to_uint(tmp_int);
	increment += sizeof(request->client_id);

	memcpy(&request->command, *buffer + increment, sizeof(request->command));
	increment += sizeof(request->command);

	memcpy(&request->flags, *buffer + increment, sizeof(request->flags));
	increment += sizeof(request->flags);

	memset(tmp_int, 0, sizeof tmp_int);
	memcpy(tmp_int, *buffer + increment,  sizeof(request->files_to_read));
	if(tmp_int[0] == 0xff && tmp_int[1] == 0xff && tmp_int[2] == 0xff && tmp_int[3] == 0xff) request->files_to_read = 0;
	else request->files_to_read = (int) char_to_uint(tmp_int);
	increment += sizeof(request->files_to_read);

	memset(tmp_int, 0, sizeof tmp_int);
	memcpy(tmp_int, *buffer + increment,  sizeof(request->pathlen));
	request->pathlen = char_to_uint(tmp_int);
	increment += sizeof(request->pathlen);

	if(request->pathlen){
		request->pathname = calloc(request->pathlen, sizeof(char));
		memcpy(request->pathname, *buffer + increment, request->pathlen);
		increment += request->pathlen;
	}

	
	memcpy(tmp_long, *buffer + increment, sizeof(request->size));
	
	request->size = char_to_ulong(tmp_long);
	increment += sizeof(request->size);
	if(request->size){
		request->data = (unsigned char *)calloc(request->size, sizeof(unsigned char));
		memcpy(request->data, *buffer + increment, request->size);
	}
	free(*buffer);
	*buffer = NULL;
	return 0;
}

int serialize_response(server_response response, unsigned char** buffer, unsigned long* buffer_len){
	int increment = 0;
	unsigned char tmp_int[sizeof(unsigned int)];
	unsigned char tmp_long[sizeof(unsigned long)];
	*buffer_len = sizeof(response.pathlen) + response.pathlen + sizeof(response.code) + sizeof(response.size) + response.size;
	*buffer = (unsigned char *) calloc(*buffer_len, sizeof(unsigned char));

	uint_to_char(response.pathlen, tmp_int);
	memcpy(*buffer, tmp_int, sizeof(response.pathlen));
	increment += sizeof(response.pathlen);

	if(response.pathlen){
		memcpy(*buffer + increment, response.pathname, response.pathlen);
		increment += response.pathlen;
	}

	memcpy(*buffer + increment, response.code, sizeof(response.code));
	increment += sizeof(response.code);

	ulong_to_char(response.size, tmp_long);
	memcpy(*buffer + increment, tmp_long, sizeof(response.size));
	increment += sizeof(response.size);

	if(response.size) memcpy(*buffer + increment, response.data, response.size);
	
	return 0;
}

int deserialize_response(server_response *response, unsigned char** buffer, unsigned long buffer_len){

	// int increment = sizeof(unsigned long);
	int increment = 0;
	unsigned char tmp_int[sizeof(unsigned int)];
	unsigned char tmp_long[sizeof(unsigned long)];
	memset(tmp_int, 0, sizeof(unsigned int));
	memset(tmp_long, 0, sizeof(unsigned long));

	memcpy(tmp_int, *buffer,  sizeof(response->pathlen));
	response->pathlen = char_to_uint(tmp_int);
	increment += sizeof(response->pathlen);

	if(response->pathlen){
		response->pathname = calloc(response->pathlen, sizeof(char));
		memcpy(response->pathname, *buffer + increment, response->pathlen);
		increment += response->pathlen;
	}
	
	memcpy(&response->code, *buffer + increment, sizeof(response->code));
	increment += sizeof(response->code);
	
	memcpy(tmp_long, *buffer + increment, sizeof(response->size));
	response->size = char_to_ulong(tmp_long);
	increment += sizeof(response->size);
	if(response->size){
		response->data = (unsigned char *)calloc(response->size, sizeof(unsigned char));
		memcpy(response->data, *buffer + increment, response->size);
	}
	free(*buffer);
	*buffer = NULL;

	return 0;
}

ssize_t readn(int fd, void *ptr, size_t n){
	size_t nleft;
	ssize_t nread;
	uint8_t *pointer = ptr;
	nleft = n;
	while (nleft > 0)
	{
		if ((nread = read(fd, pointer, nleft)) < 0)
		{
			if (nleft == n){
				ptr = pointer;
				return -1; /* error, return -1 */
			}
			else
				break; /* error, return amount read so far */
		}
		else if (nread == 0)
			break; /* EOF */
		nleft -= nread;
		pointer += nread;
	}
	ptr = pointer;
	return (n - nleft); /* return >= 0 */
}


ssize_t writen(int fd, void *ptr, size_t n){
	size_t nleft;
	ssize_t nwritten;
	uint8_t *pointer = ptr;
	nleft = n;
	while (nleft > 0)
	{
		if ((nwritten = write(fd, pointer, nleft)) < 0)
		{
			if (nleft == n){
				ptr = pointer;
				return -1; /* error, return -1 */
			}
			else
				break; /* error, return amount written so far */
		}
		else if (nwritten == 0)
			break;
		nleft -= nwritten;
		pointer += nwritten;
	}
	ptr = pointer;
	return (n - nleft); /* return >= 0 */
}


void reset_buffer(unsigned char** buffer, size_t* buff_size){
	if(*buffer){
		free(*buffer);
		*buffer = NULL;
	}
	*buff_size = 0;
}

void init_request(client_request* request, pid_t pid, unsigned char command, unsigned char flags, const char* pathname){
	memset(request, 0, sizeof(client_request));
	request->client_id = pid;
	request->command = command;
	request->flags = flags;
	request->pathlen = pathname ? strlen(pathname) + 1 : 0;
	if(request->pathlen){
		request->pathname = calloc(request->pathlen, sizeof(char));
		strncpy(request->pathname, pathname, strlen(pathname));
	}
}

void clean_request(client_request* request){
	if(request->data){
		free(request->data);
		request->data = NULL;
	} 
	if(request->pathname){
		free(request->pathname);
		request->pathname = NULL;
	}
}
void clean_response(server_response* response){
	if(response->data){
		free(response->data);
		response->data = NULL;
	} 
	if(response->pathname){
		free(response->pathname);
		response->pathname = NULL;
	} 
}