#include "PredictionInterpreter.h"
#include <algorithm>
#include <iostream>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <esp_log.h>

static const char *TAG = "[PredictionInterpreter]";

std::vector<float> PredictionInterpreter::GetResult(const TfLiteTensor* output_tensor, float threshold) {
	std::vector<float> results(output_tensor->bytes / GetTypeSize(output_tensor->type));

	// Handle quantized output
	Dequantize(output_tensor, results);

	// Remove elements with scores below the threshold
	for (auto it = results.begin(); it != results.end();) {
		if (*it < threshold) {
			it = results.erase(it);
		} 
		else {
			++it;
		}
	}
	return results;
}

void PredictionInterpreter::Dequantize(const TfLiteTensor* output_tensor, std::vector<float>& results) {
	int size = results.size();
	float scale = output_tensor->params.scale;
	int zero_point = output_tensor->params.zero_point;

	// Scale should not be zero in the case of quantized tensors
	if (output_tensor->quantization.type != kTfLiteNoQuantization && scale == 0) {
		ESP_LOGE(TAG, "Scale is zero, invalid quantization parameters");
		vTaskDelete(NULL);
	}

	switch (output_tensor->type) {
		case kTfLiteFloat32: {
			const float* data = output_tensor->data.f;
			for (int i = 0; i < size; i++) {
				results[i] = data[i];
			}
			break;
		}
		case kTfLiteUInt8: {
			const uint8_t* data = output_tensor->data.uint8;
			if (output_tensor->quantization.type == kTfLiteNoQuantization) {
				for (int i = 0; i < size; i++) {
					results[i] = data[i];
				}
			}
			else {
				for (int i = 0; i < size; i++) {
					results[i] = (data[i] - zero_point) * scale;
				}
			}
			break;
		}
		case kTfLiteInt8: {
			const int8_t* data = output_tensor->data.int8;
			if (output_tensor->quantization.type == kTfLiteNoQuantization) {
				for (int i = 0; i < size; i++) {
					results[i] = data[i];
				}
			}
			else {
				for (int i = 0; i < size; i++) {
					results[i] = (data[i] - zero_point) * scale;
				}
			}
			break;
		}
		case kTfLiteInt16: {
			const int16_t* data = output_tensor->data.i16;
			if (output_tensor->quantization.type == kTfLiteNoQuantization) {
				for (int i = 0; i < size; i++) {
					results[i] = data[i];
				}
			}
			else {
				for (int i = 0; i < size; i++) {
					results[i] = (data[i] - zero_point) * scale;
				}
			}
			break;
		}
		case kTfLiteInt32: {
			const int32_t* data = output_tensor->data.i32;
			for (int i = 0; i < size; i++) {
				results[i] = static_cast<float>(data[i]);
			}
			break;
		}
		case kTfLiteBool: {
			const bool* data = output_tensor->data.b;
			for (int i = 0; i < size; i++) {
				results[i] = data[i] ? 1.0f : 0.0f;
			}
			break;
		}
		default: {
			ESP_LOGE(TAG, "Unsupported tensor type: %d", output_tensor->type);
			vTaskDelete(NULL);
		}
	}
}

size_t PredictionInterpreter::GetTypeSize(TfLiteType type) {
	switch (type) {
		case kTfLiteFloat32:
			return sizeof(float);
		case kTfLiteUInt8:
			return sizeof(uint8_t);
		case kTfLiteInt8:
			return sizeof(int8_t);
		case kTfLiteInt16:
			return sizeof(int16_t);
		case kTfLiteInt32:
			return sizeof(int32_t);
		case kTfLiteBool:
			return sizeof(bool);
		default:
			ESP_LOGE(TAG, "Unsupported tensor type: %d", type);
			vTaskDelete(NULL);
			return 0; // This line will never be reached, but it avoids a compiler warning
	}
}
