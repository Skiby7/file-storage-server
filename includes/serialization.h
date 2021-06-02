#include "common_includes.h"
#include "connections.h"

int serialize_request(client_request request, unsigned char** buffer, unsigned long* buffer_len);
int deserialize_request(client_request *request, unsigned char** buffer, unsigned long buffer_len);
int serialize_response(server_response response, unsigned char** buffer, unsigned long* buffer_len);
int deserialize_response(server_response *response, unsigned char** buffer, unsigned long buffer_len);
void reset_buffer(unsigned char** buffer, int* buff_size);
ssize_t readn(int fd, void *ptr, size_t n);
ssize_t writen(int fd, void *ptr, size_t n);