#ifndef TCP_SERVER_H
#define TCP_SERVER_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PORT 1234

typedef struct {
	int server_fd;
	struct sockaddr_in address;
	int addrlen;
} tcp_server_t;

int tcp_server_init(tcp_server_t *server);
int tcp_server_accept(tcp_server_t *server);
ssize_t tcp_server_receive(int client_socket, void *buffer, size_t buffer_size);
ssize_t tcp_server_send(int client_socket, const void *buffer, size_t buffer_size);

#ifdef __cplusplus
}
#endif

#endif // TCP_SERVER_H