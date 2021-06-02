#include "serialization.h"

void ulong_to_char(unsigned long integer, unsigned char **array){
	int n = sizeof(unsigned long);
	unsigned char temp[n];
	memset(temp, 0, n);
	*array = (unsigned char *)calloc(n, sizeof(unsigned char));
	n *= 8;
	n -= 8;

	for(int i = 0; i < sizeof(unsigned long); i++, n -= 8)
    	temp[i] = (integer >> n) & 0xff;
	memcpy(*array, temp, sizeof(unsigned long));
}


void uint_to_char(unsigned int integer, unsigned char **array){
	int n = sizeof(unsigned int);
	unsigned char temp[n];
	memset(temp, 0, n);
	*array = (unsigned char *)calloc(n, sizeof(unsigned char));
	n *= 8;
	n -= 8;
	for(int i = 0; i < sizeof(unsigned int); i++, n -= 8)
		temp[i] = (integer >> n) & 0xff;
    	
	memcpy(*array, temp, sizeof(unsigned int));
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
	unsigned long integer = 0;
	for(int i = 0; i < sizeof(unsigned long); i++, n -= 8)
		integer |= ((unsigned long) array[i] << n);


	return integer;
}

int serialize_request(client_request request, unsigned char** buffer, unsigned long* buffer_len){
	int increment = 0;
	unsigned char *tmp = NULL;
	*buffer_len = sizeof(request.client_id) + sizeof(request.command) + sizeof(request.flags) + sizeof(request.pathname) + sizeof(request.size) + request.size;
	*buffer = (unsigned char *) calloc(*buffer_len, sizeof(unsigned char));

	uint_to_char(request.client_id, &tmp);
	memcpy(*buffer, tmp, sizeof(request.client_id));
	free(tmp);
	increment += sizeof(request.client_id);

	memcpy(*buffer + increment, &request.command, sizeof(request.command));
	increment += sizeof(request.command);

	memcpy(*buffer + increment, &request.flags, sizeof(request.flags));
	increment += sizeof(request.flags);

	memcpy(*buffer + increment, &request.pathname, sizeof(request.pathname));
	increment += sizeof(request.pathname);

	ulong_to_char(request.size, &tmp);

	memcpy(*buffer + increment, tmp, sizeof(request.size));
	free(tmp);
	increment += sizeof(request.size);

	memcpy(*buffer + increment, request.data, request.size);
	
	return 0;
}

int deserialize_request(client_request *request, unsigned char** buffer, unsigned long buffer_len){

	int increment = 0;
	unsigned char tmp[sizeof(unsigned long)];
	memset(tmp, 0, sizeof(unsigned char));
	
	memcpy(tmp, *buffer,  sizeof(request->client_id));
	request->client_id = char_to_uint(tmp);
	memset(tmp, 0, sizeof(unsigned char));
	increment += sizeof(request->client_id);

	memcpy(&request->command, *buffer + increment, sizeof(request->command));
	increment += sizeof(request->command);

	memcpy(&request->flags, *buffer + increment, sizeof(request->flags));
	increment += sizeof(request->flags);

	memcpy(&request->pathname, *buffer + increment, sizeof(request->pathname));
	increment += sizeof(request->pathname);
	
	memcpy(tmp, *buffer + increment, sizeof(request->size));
	
	request->size = char_to_ulong(tmp);
	increment += sizeof(request->size);
	printf("%lu\n", request->size);
	request->data = (unsigned char *)calloc(request->size, sizeof(unsigned char));
	memcpy(request->data, *buffer + increment, request->size);
	free(*buffer);
	*buffer = NULL;

	return 0;
}

int serialize_response(server_response response, unsigned char** buffer, unsigned long* buffer_len){
	int increment = 0;
	unsigned char *tmp = NULL;
	*buffer_len = sizeof(response.filename) + sizeof(response.code) + sizeof(response.size) + response.size;
	*buffer = (unsigned char *) calloc(*buffer_len, sizeof(unsigned char));

	memcpy(*buffer, &response.filename, sizeof(response.filename));
	increment += sizeof(response.filename);

	uint_to_char(response.code, &tmp);
	memcpy(*buffer + increment, tmp, sizeof(response.code));
	free(tmp);
	increment += sizeof(response.code);

	ulong_to_char(response.size, &tmp);
	memcpy(*buffer + increment, tmp, sizeof(response.size));
	free(tmp);
	increment += sizeof(response.size);

	memcpy(*buffer + increment, response.data, response.size);
	
	return 0;
}

int deserialize_response(server_response *response, unsigned char** buffer, unsigned long buffer_len){

	int increment = 0;
	unsigned char tmp[sizeof(unsigned long)];
	memset(tmp, 0, sizeof(unsigned char));

	memcpy(&response->filename, buffer + increment, sizeof(response->filename));
	increment += sizeof(response->filename);

	memcpy(&response->code, buffer + increment, sizeof(response->code));
	increment += sizeof(response->code);
	
	memcpy(tmp, buffer + increment, sizeof(response->size));
	response->size = char_to_ulong(tmp);
	increment += sizeof(response->size);
	
	response->data = (unsigned char *)calloc(response->size, sizeof(unsigned char));
	memcpy(response->data, buffer + increment, response->size);
	free(*buffer);
	*buffer = NULL;

	return 0;
}

ssize_t readn(int fd, void *ptr, size_t n){
	size_t nleft;
	ssize_t nread;

	nleft = n;
	while (nleft > 0)
	{
		if ((nread = read(fd, ptr, nleft)) < 0)
		{
			if (nleft == n)
				return -1; /* error, return -1 */
			else
				break; /* error, return amount read so far */
		}
		else if (nread == 0)
			break; /* EOF */
		nleft -= nread;
		ptr += nread;
	}
	return (n - nleft); /* return >= 0 */
}

ssize_t writen(int fd, void *ptr, size_t n){
	size_t nleft;
	ssize_t nwritten;

	nleft = n;
	while (nleft > 0)
	{
		if ((nwritten = write(fd, ptr, nleft)) < 0)
		{
			if (nleft == n)
				return -1; /* error, return -1 */
			else
				break; /* error, return amount written so far */
		}
		else if (nwritten == 0)
			break;
		nleft -= nwritten;
		ptr += nwritten;
	}
	return (n - nleft); /* return >= 0 */
}





void reset_buffer(unsigned char** buffer, int* buff_size){
	free(*buffer);
	*buffer = NULL;
	*buff_size = 0;
}