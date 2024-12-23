#include "tcp_server.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_netif.h"

static const char *TAG = "[tcp_server]";

int tcp_server_init(tcp_server_t *server) {
	int opt = 1;

	// Create socket
	if ((server->server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
		ESP_LOGE(TAG, "socket failed: errno %d", errno);
		return -1;
	}

	// Set socket options
	if (setsockopt(server->server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
		ESP_LOGE(TAG, "setsockopt failed: errno %d", errno);
		close(server->server_fd);
		return -1;
	}

	// Bind socket to port
	server->address.sin_family = AF_INET;
	server->address.sin_addr.s_addr = INADDR_ANY;
	server->address.sin_port = htons(PORT);
	server->addrlen = sizeof(server->address);

	if (bind(server->server_fd, (struct sockaddr *)&server->address, sizeof(server->address)) < 0) {
		ESP_LOGE(TAG, "bind failed: errno %d", errno);
		close(server->server_fd);
		return -1;
	}

	// Listen for connections
	if (listen(server->server_fd, 3) < 0) {
		ESP_LOGE(TAG, "listen failed: errno %d", errno);
		close(server->server_fd);
		return -1;
	}

	ESP_LOGI(TAG, "Server is listening on port %d", PORT);
	return 0;
}

int tcp_server_accept(tcp_server_t *server) {
	int new_socket;
	if ((new_socket = accept(server->server_fd, (struct sockaddr *)&server->address, (socklen_t *)&server->addrlen)) < 0) {
		ESP_LOGE(TAG, "accept failed: errno %d", errno);
		return -1;
	}
	return new_socket;
}

ssize_t tcp_server_receive(int client_socket, void *buffer, size_t buffer_size) {
	ssize_t total_size = 0, size = 0;
	while (total_size < buffer_size) {
		size = recv(client_socket, buffer + total_size, buffer_size - total_size, 0);
		if (size < 0) {
			ESP_LOGE(TAG, "recv failed: errno %d", errno);
			return -1;
		}
		total_size += size;
	}
	return total_size;
}

ssize_t tcp_server_send(int client_socket, const void *buffer, size_t buffer_size) {
	ssize_t total_size = 0, size = 0;
	while (total_size < buffer_size) {
		size = send(client_socket, buffer + total_size, buffer_size - total_size, 0);
		if (size < 0) {
			ESP_LOGE(TAG, "send failed: errno %d", errno);
			return -1;
		}
		total_size += size;
	}
	return total_size;
}