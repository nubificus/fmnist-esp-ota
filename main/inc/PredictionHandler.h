#pragma once

#include <vector>
#include <string>
#include <utility>

// #include "tcp_client.h"
#include "tcp_server.h"

class PredictionHandler {
	public:
	int Update(int client_socket, const std::vector<float>& predictions, long long inference_time);
};