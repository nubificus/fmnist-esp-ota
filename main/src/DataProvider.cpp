#include "DataProvider.h"

#include <esp_log.h>

#include <vector>
#include <string>
#include <iostream>

static const char *TAG = "[tcp_server]";

int DataProvider::Read(int client_socket, TfLiteTensor* modelInput) {
	char request_byte = 0x00;

	// Read the request byte
	int err = tcp_server_receive(client_socket, &request_byte, 1);
	if (err <= 0) {
		ESP_LOGE(TAG, "Error occurred during receiving request byte: errno %d", errno);
		return 1;
	}

	// Check the request byte
	if (request_byte != 0x01) {
		ESP_LOGE(TAG, "Invalid request byte: %d", request_byte);
		return 1;
	}

	// Read the image data
	err = tcp_server_receive(client_socket, modelInput->data.raw, modelInput->bytes);
	if (err <= 0) {
		ESP_LOGE(TAG, "Error occurred during receiving image: errno %d", errno);
		return 1;
	}

	return 0;
}