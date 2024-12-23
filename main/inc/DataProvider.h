#pragma once

#include "tensorflow/lite/c/common.h"
#include "tcp_server.h"

class DataProvider {
	public:
	int Read(int client_socket, TfLiteTensor* modelInput);
};