#include "PredictionHandler.h"

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_log.h>

#include <iostream>
#include <iomanip>

static const char *TAG = "[tcp_server]";

int PredictionHandler::Update(int client_socket, const std::vector<float>& predictions, long long inference_time) {
	int err;
	
	err = tcp_server_send(client_socket, (void*) predictions.data(), predictions.size() * sizeof(float));
	if (err < 0) {
		ESP_LOGE(TAG, "Failed to send inference result to client");
		return 1;
	}

	err = tcp_server_send(client_socket, (void*) &inference_time, sizeof(inference_time));
	if (err < 0) {
		ESP_LOGE(TAG, "Failed to send inference time to client");
		return 1;
	}

	return 0;
}