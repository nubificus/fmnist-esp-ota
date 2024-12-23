#include "PredictionHandler.h"

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_log.h>

#include <iostream>
#include <iomanip>

static const char *TAG = "[tcp_server]";

int PredictionHandler::Update(int client_socket, const std::vector<float>& predictions, long long inference_time) {
	int bytes_to_send = predictions.size() * sizeof(float);
	int bytes_sent = tcp_server_send(client_socket, (void*) predictions.data(), bytes_to_send);	
	
	if (bytes_sent != bytes_to_send) {
		ESP_LOGE(TAG, "Failed to send inference result to client");
		return 1;
	}

	bytes_sent = tcp_server_send(client_socket, (void*) &inference_time, sizeof(inference_time));
	if (bytes_sent !=  sizeof(inference_time)) {
		ESP_LOGE(TAG, "Failed to send inference time to client");
		return 1;
	}

	return 0;
}