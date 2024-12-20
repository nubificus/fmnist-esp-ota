#include "main_functions.h"

#include "DataProvider.h"
#include "PredictionHandler.h"
#include "PredictionInterpreter.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "micro_model.h"
#include "tensorflow/lite/micro/all_ops_resolver.h"
#include "tensorflow/lite/micro/kernels/micro_ops.h"
#include "tensorflow/lite/micro/micro_error_reporter.h"
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/micro/micro_mutable_op_resolver.h"
#include "tensorflow/lite/schema/schema_generated.h"
#include "esp_timer.h"
#include <esp_log.h>

// delay connstant -> 1 sec
#define pdSECOND pdMS_TO_TICKS(1000)

namespace {
	// declare ErrorReporter, a TfLite class for error logging
	tflite::ErrorReporter *error_reporter = nullptr;
	// declare the model that will hold the generated C array
	const tflite::Model *model = nullptr;
	// declare interpreter, runs inference using model and data
	tflite::MicroInterpreter *interpreter = nullptr;
	// declare model input and output as 1D-arrays
	TfLiteTensor *model_input = nullptr;
	TfLiteTensor *model_output = nullptr;
	// create an area of memory to use for input, output, and intermediate arrays.
	// the size of this will depend on the model you're using, and may need to be
	// determined by experimentation.
	constexpr int kTensorArenaSize = 32 * 1024;
	alignas(16) uint8_t tensor_arena[kTensorArenaSize];

	static const char *TAG = "[esp_cli]";

	// processing pipeline
	DataProvider data_provider;
	PredictionInterpreter prediction_interpreter;
	PredictionHandler prediction_handler;
} // namespace

void setup() {
	static tflite::MicroErrorReporter micro_error_reporter;
	error_reporter = &micro_error_reporter;

	// import the trained weights from the C array
	model = tflite::GetModel(fmnist_frozen_micro_model_cc_data);

	if (model->version() != TFLITE_SCHEMA_VERSION) {
		TF_LITE_REPORT_ERROR(error_reporter,
							"Model provided is schema version %d not equal "
							"to supported version %d.",
							model->version(), TFLITE_SCHEMA_VERSION);
		return;
	}

	// load all tflite micro built-in operations
	// for example layers, activation functions, pooling
	static tflite::AllOpsResolver resolver;

	// initialize interpreter
	static tflite::MicroInterpreter static_interpreter(
		model, resolver, tensor_arena, kTensorArenaSize, error_reporter);
	interpreter = &static_interpreter;

	// interpreter allocates memory according to model requirements
	TfLiteStatus allocate_status = interpreter->AllocateTensors();
	if (allocate_status != kTfLiteOk) {
		TF_LITE_REPORT_ERROR(error_reporter, "AllocateTensors() failed\n");
		return;
	}

	model_input = interpreter->input(0);
	model_output = interpreter->output(0);

	if ((model_input->dims->size != 4 || (model_input->dims->data[0] != 1) ||
		(model_input->dims->data[1] != 28) ||
		(model_input->dims->data[2] != 28) || (model_input->dims->data[3] != 1) ||
		(model_input->type != kTfLiteFloat32))) {
		error_reporter->Report("Bad input tensor parameters in model\n");
		return;
	}

	// initialize periphery
	if (!data_provider.Init()) {
		error_reporter->Report("Failed to initialize data provider\n");
	}
}

void loop() {
	// Read test data and copy them to the model input tensor
	if (data_provider.Read(model_input)) {
		error_reporter->Report("Failed ro read next image");
		return;
	}

	// Run inference on pre-processed data
	long long start_time = esp_timer_get_time();

	TfLiteStatus invoke_status = interpreter->Invoke();
	if (invoke_status != kTfLiteOk) {
		error_reporter->Report("Invoke failed");
		return;
	}

	long long inference_time = esp_timer_get_time() - start_time;

	// Interpret raw model predictions
	auto prediction = prediction_interpreter.GetResult(model_output, 0.0);

	// Send the result to the server
	prediction_handler.Update(prediction, data_provider.sock);

	// Send inference time to the server
	if (resp(data_provider.sock, (void*) &inference_time, sizeof(inference_time)) < 0) {
		ESP_LOGE(TAG, "Failed to send response to server");
		close(data_provider.sock);
		vTaskDelete(NULL);
	}

	vTaskDelay(0.5 * pdSECOND);
}
