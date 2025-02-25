#include "DataProvider.h"

#include <esp_log.h>

#include <vector>
#include <string>
#include <iostream>

static const char *TAG = "tcp_server";

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


	if (modelInput->type == kTfLiteInt8) {
		float scale = modelInput->params.scale;
		int zero_point = modelInput->params.zero_point;
		int8_t* input_data = modelInput->data.int8;

		// Read the image data
		std::vector<float> input_data_vec(modelInput->bytes * sizeof(float));
		err = tcp_server_receive(client_socket, input_data_vec.data(), modelInput->bytes * sizeof(float));
		if (err <= 0) {
			ESP_LOGE(TAG, "Error occurred during receiving image: errno %d", errno);
			return 1;
		}

		// Convert float to int8
		for (size_t i = 0; i < input_data_vec.size(); i++) {
			input_data[i] = static_cast<int8_t>((input_data_vec[i] / scale) + zero_point);
		}
	} else if (modelInput->type == kTfLiteFloat32) {
		// Read the image data
		err = tcp_server_receive(client_socket, modelInput->data.raw, modelInput->bytes);
		if (err <= 0) {
			ESP_LOGE(TAG, "Error occurred during receiving image: errno %d", errno);
			return 1;
		}
	} else {
		ESP_LOGE(TAG, "Input tensor type is not supported: %d", modelInput->type);
		return 1;
	}

	return 0;
}