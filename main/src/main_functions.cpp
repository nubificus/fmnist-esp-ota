#include "main_functions.h"

#include "DataProvider.h"
#include "PredictionHandler.h"
#include "PredictionInterpreter.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "micro_model.h"
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/micro/micro_log.h"
#include "tensorflow/lite/micro/micro_mutable_op_resolver.h"
#include "tensorflow/lite/schema/schema_generated.h"
#include "tensorflow/lite/micro/kernels/micro_ops.h"
#include "tensorflow/lite/micro/tflite_bridge/micro_error_reporter.h"
#include "esp_timer.h"
#include <esp_log.h>

#include "tcp_server.h"

// delay connstant -> 1 sec
#define pdSECOND pdMS_TO_TICKS(1000)

namespace {
	// Declare ErrorReporter, a TfLite class for error logging
	tflite::ErrorReporter *error_reporter = nullptr;
	// Declare the model that will hold the generated C array
	const tflite::Model *model = nullptr;
	// Declare interpreter, runs inference using model and data
	tflite::MicroInterpreter *interpreter = nullptr;
	
	// Declare model input and output tensor pointers
	TfLiteTensor *model_input = nullptr;
	TfLiteTensor *model_output = nullptr;
	
	// Create an area of memory to use for input, output, and intermediate arrays.
	// the size of this will depend on the model you're using, and may need to be
	// determined by experimentation.
	constexpr int kTensorArenaSize = 152 * 1024;
	alignas(16) uint8_t tensor_arena[kTensorArenaSize];

	// Processing pipeline
	DataProvider data_provider;
	PredictionInterpreter prediction_interpreter;
	PredictionHandler prediction_handler;
}

void PerformWarmup(int warmup_runs) {
	// Fill input tensor with dummy data (ones)
	memset(model_input->data.raw, 1, model_input->bytes);
	
	for (int i = 0; i < warmup_runs; i++) {
		if (interpreter->Invoke() != kTfLiteOk) {
			error_reporter->Report("Warmup inference failed on iteration %d", i + 1);
			return;
		}
		vTaskDelay(0.5 * pdSECOND);
	}
	ESP_LOGI("[setup]", "Completed %d warmup runs.", warmup_runs);
}


void setup(tcp_server_t *server) {
	static tflite::MicroErrorReporter micro_error_reporter;
	error_reporter = &micro_error_reporter;

	// Import the trained weights from the C array
	model = tflite::GetModel(micro_model_cc_data);

	// Check if the model is compatible with the TensorFlow Lite interpreter
	if (model->version() != TFLITE_SCHEMA_VERSION) {
		error_reporter->Report("Model provided is schema version %d not equal "
							   "to supported version %d.",
								model->version(), TFLITE_SCHEMA_VERSION);
		vTaskDelete(NULL);
	}

	// load all tflite micro built-in operations
	// for example layers, activation functions, pooling
	static tflite::MicroMutableOpResolver<7> micro_op_resolver;
	if (micro_op_resolver.AddConv2D() != kTfLiteOk) {
		error_reporter->Report("AddConv2D failed");
		vTaskDelete(NULL);
	}
	if (micro_op_resolver.AddMaxPool2D() != kTfLiteOk) {
		error_reporter->Report("AddMaxPool2D failed");
		vTaskDelete(NULL);
	}
	if (micro_op_resolver.AddFullyConnected() != kTfLiteOk) {
		error_reporter->Report("AddFullyConnected failed");
		vTaskDelete(NULL);
	}
	if (micro_op_resolver.AddSoftmax() != kTfLiteOk) {
		error_reporter->Report("AddSoftmax failed");
		vTaskDelete(NULL);
	}//
#if 0
	if (micro_op_resolver.AddReshape() != kTfLiteOk) {
		error_reporter->Report("AddReshape failed");
		vTaskDelete(NULL);
	}
#endif
	if (micro_op_resolver.AddMean() != kTfLiteOk) {
	 	error_reporter->Report("AddMean failed");
	 	vTaskDelete(NULL);
	}
	if (micro_op_resolver.AddAdd() != kTfLiteOk) {
		error_reporter->Report("AddAdd failed");
	 	vTaskDelete(NULL);
	}
	if (micro_op_resolver.AddMul() != kTfLiteOk) {
	 	error_reporter->Report("AddMul failed");
	 	vTaskDelete(NULL);
	}

	// Build an interpreter to run the model with.
	static tflite::MicroInterpreter static_interpreter(
		model, micro_op_resolver, tensor_arena, kTensorArenaSize);
	interpreter = &static_interpreter;

	// Allocate tensor buffers
	TfLiteStatus allocate_status = interpreter->AllocateTensors();
	if (allocate_status != kTfLiteOk) {
		error_reporter->Report("AllocateTensors() failed");
		vTaskDelete(NULL);
	}

	// Get pointers to the input and output tensors
	model_input = interpreter->input(0);
	model_output = interpreter->output(0);

	// Perform warmup runs before measuring inference time
	ESP_LOGI("[setup]", "Performing warmup runs...");
	int warmup_runs = 10;
	PerformWarmup(warmup_runs);

	// Initialize the ESP32 server
	int err = tcp_server_init(server);
	if (err  == -1) {
		error_reporter->Report("Failed to Start Server");
		vTaskDelete(NULL);
	}
}

void handle_client(void *args) {
	int client_socket = (int)args;
	while(1) {
		// Read test data and copy them to the model input tensor
		if (data_provider.Read(client_socket, model_input)) {
			break;
		}

		// Run inference on pre-processed data
		long long start_time = esp_timer_get_time();

		TfLiteStatus invoke_status = interpreter->Invoke();
		if (invoke_status != kTfLiteOk) {
			error_reporter->Report("Invoke failed");
			break;
		}

		long long inference_time = esp_timer_get_time() - start_time;

		// Interpret raw model predictions
		auto prediction = prediction_interpreter.GetResult(model_output, 0.0);

		// Send the inference result to the client
		if (prediction_handler.Update(client_socket, prediction, inference_time)) {
			break;
		}

		vTaskDelay(0.5 * pdSECOND);
	}

	close(client_socket);
	vTaskDelete(NULL);
}

void loop(tcp_server_t *server) {
	const char *TAG = "[tcp_server]";
	while (1) {
		ESP_LOGI(TAG, "Waiting for client connection...");
		int client_socket = tcp_server_accept(server);
		if (client_socket < 0) {
			ESP_LOGE(TAG, "Failed to accept client connection");
			continue;
		}

		// Handle the client connection in a separate task
		ESP_LOGI(TAG, "New client connected");
		xTaskCreate(handle_client, "handle_client", 4096, (void *)client_socket, 5, NULL);
	}
}
