#include "main_functions.h"

#include "DataProvider.h"
#include "PredictionHandler.h"
#include "PredictionInterpreter.h"

#ifndef LOAD_MODEL_FROM_PARTITION
#include "micro_model.h"
#endif

#include "micro_ops.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/micro/micro_log.h"
#include "tensorflow/lite/schema/schema_generated.h"
#include "tensorflow/lite/micro/kernels/micro_ops.h"
#include "tensorflow/lite/micro/tflite_bridge/micro_error_reporter.h"

#include "esp_timer.h"
#include "esp_log.h"
#include "esp_task_wdt.h"

#include "esp_heap_caps.h"

#ifdef ENABLE_PSRAM 
#include "esp_psram.h"
#endif

#ifdef LOAD_MODEL_FROM_PARTITION
#include "esp_partition.h"
#endif

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
	constexpr int kTensorArenaSize = (TENSOR_ALLOCATION_SPACE);
	uint8_t *tensor_arena = nullptr;

	// Processing pipeline
	DataProvider data_provider;
	PredictionInterpreter prediction_interpreter;
	PredictionHandler prediction_handler;
}

void PerformWarmup(int warmup_runs) {
	// Increase watchdog timeout to 20 seconds
	esp_task_wdt_config_t config = {
		.timeout_ms = 20000,  // Set timeout to 20 sec
		.idle_core_mask = (1 << 0) | (1 << 1),  // Apply to both cores
		.trigger_panic = false  // Don't trigger panic, just log warning
	};

	esp_task_wdt_reconfigure(&config);

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

	// Restore watchdog timeout to default (5 sec)
	config.timeout_ms = 5000,  // Restore timeout to 5 sec
	esp_task_wdt_reconfigure(&config);
}

void allocate_tensor_arena() {
#ifndef ENABLE_PSRAM
	// Allocate tensor arena in internal RAM
	tensor_arena = (uint8_t *) heap_caps_malloc(kTensorArenaSize, MALLOC_CAP_INTERNAL);
	if (tensor_arena) {
		ESP_LOGI("allocate_tensor_arena", "Tensor arena allocated in internal RAM (%d bytes)", kTensorArenaSize);
	} else {
		ESP_LOGE("allocate_tensor_arena", "Failed to allocate tensor arena in internal RAM!");
		vTaskDelete(NULL);
	}
#else
	// Allocate tensor arena in external PSRAM
	if (esp_psram_is_initialized()) {
		ESP_LOGI("allocate_tensor_arena", "PSRAM is available! Total size: %d bytes", esp_psram_get_size());
		
		tensor_arena = (uint8_t *) heap_caps_malloc(kTensorArenaSize, MALLOC_CAP_SPIRAM);
		if (tensor_arena) {
			ESP_LOGI("allocate_tensor_arena", "Tensor arena allocated in PSRAM (%d bytes)", kTensorArenaSize);
		} else {
			ESP_LOGE("allocate_tensor_arena", "Failed to allocate tensor arena in PSRAM!");
			vTaskDelete(NULL);
		}
	} else { // PSRAM is not available -> Try internal RAM
		ESP_LOGW("allocate_tensor_arena", "PSRAM is NOT available! Trying internal RAM.");
		
		tensor_arena = (uint8_t *) heap_caps_malloc(kTensorArenaSize, MALLOC_CAP_INTERNAL);
		if (tensor_arena) {
			ESP_LOGI("allocate_tensor_arena", "Tensor arena allocated in internal RAM (%d bytes)", kTensorArenaSize);
		} else {
			ESP_LOGE("allocate_tensor_arena", "Failed to allocate tensor arena in internal RAM!");
			vTaskDelete(NULL);
		}
	}
#endif
	vTaskDelay(0.5 * pdSECOND);
}

#ifdef LOAD_MODEL_FROM_PARTITION
const tflite::Model* load_model_from_partition() {
	// Find the partition that contains the model
	const esp_partition_t* partition = esp_partition_find_first(
		ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_ANY, "tflite_model");

	if (!partition) {
		ESP_LOGE("load_model_from_partition", "Model partition not found!");
		return nullptr;
	}

	const void* model_data;
	esp_partition_mmap_handle_t mmap_handle;
	
	// Start from the beginning of the partition
	size_t offset = 0;
	// The model size is an environment variable set by the user
	size_t model_size = (TFLITE_MODEL_SIZE);

	// Map the model partition to memory
	esp_err_t err = esp_partition_mmap(
						partition,
						offset,
						model_size,
						ESP_PARTITION_MMAP_DATA,
						&model_data,
						&mmap_handle
					);

	if (err != ESP_OK) {
		ESP_LOGE("load_model_from_partition", "Failed to map model partition!");
		return nullptr;
	}

	ESP_LOGI("load_model_from_partition", "Model successfully mapped from flash");
	return tflite::GetModel(model_data);
}
#endif

void setup(tcp_server_t *server) {
	static tflite::MicroErrorReporter micro_error_reporter;
	error_reporter = &micro_error_reporter;
	
	// Allocate memory for the tensor arena
	allocate_tensor_arena();

	// Load the tflite model
#ifdef LOAD_MODEL_FROM_PARTITION
	model = load_model_from_partition();
#else
	model = tflite::GetModel(micro_model_cc_data);
#endif

	// Check if the model is loaded
	if (!model) {
		error_reporter->Report("Failed to load tflite model");
		vTaskDelete(NULL);
	}

	// Check if the model is compatible with the TensorFlow Lite interpreter
	if (model->version() != TFLITE_SCHEMA_VERSION) {
		error_reporter->Report("Model provided is schema version %d not equal "
							   "to supported version %d.",
								model->version(), TFLITE_SCHEMA_VERSION);
		vTaskDelete(NULL);
	}

	// Get micro op resolver generated for this model
	auto* micro_op_resolver = get_micro_op_resolver(error_reporter);

	// Build an interpreter to run the model with.
	static tflite::MicroInterpreter static_interpreter(
		model, *micro_op_resolver, tensor_arena, kTensorArenaSize);
	interpreter = &static_interpreter;

	// Allocate tensor buffers
	TfLiteStatus allocate_status = interpreter->AllocateTensors();
	if (allocate_status != kTfLiteOk) {
		error_reporter->Report("AllocateTensors() failed");
		vTaskDelete(NULL);
	}

	// Show the memory usage of the model
	ESP_LOGI("setup", "Used tensor arena: %d bytes", interpreter->arena_used_bytes());

	// Get pointers to the input and output tensors
	model_input = interpreter->input(0);
	model_output = interpreter->output(0);

	// Perform warmup runs before measuring inference time
	ESP_LOGI("setup", "Performing warmup runs...");
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

	// Increase watchdog timeout to 20 seconds
	esp_task_wdt_config_t config = {
		.timeout_ms = 20000,  // Set timeout to 20 sec
		.idle_core_mask = (1 << 0) | (1 << 1),  // Apply to both cores
		.trigger_panic = false  // Don't trigger panic, just log warning
	};

	esp_task_wdt_reconfigure(&config);

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

	// Restore watchdog timeout to default (5 sec)
	config.timeout_ms = 5000,  // Restore timeout to 5 sec
	esp_task_wdt_reconfigure(&config);
 
	close(client_socket);
	vTaskDelete(NULL);
}

void loop(tcp_server_t *server) {
	const char *TAG = "tcp_server";
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
