#pragma once

#include <vector>
#include <string>
#include "tensorflow/lite/c/common.h"

class PredictionInterpreter {
public:
	std::vector<float> GetResult(const TfLiteTensor* output_tensor, float threshold);

private:
	size_t GetTypeSize(TfLiteType type);
	void Dequantize(const TfLiteTensor* output_tensor, std::vector<float>& results);
};
