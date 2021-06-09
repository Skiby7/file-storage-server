#include "common_includes.h"
#ifndef CONNECTIONS_H_
#define CONNECTIONS_H_
#include "connections.h"
#endif

int serialize_request(client_request request, unsigned char** buffer, unsigned long* buffer_len);
int deserialize_request(client_request *request, unsigned char** buffer, unsigned long buffer_len);
int serialize_response(server_response response, unsigned char** buffer, unsigned long* buffer_len);
int deserialize_response(server_response *response, unsigned char** buffer, unsigned long buffer_len);
unsigned long char_to_ulong(unsigned char *array);
void ulong_to_char(unsigned long integer, unsigned char **array);
void reset_buffer(unsigned char** buffer, size_t* buff_size);
ssize_t readn(int fd, void *ptr, size_t n);
ssize_t writen(int fd, void *ptr, size_t n);