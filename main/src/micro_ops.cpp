#include "tensorflow/lite/micro/micro_mutable_op_resolver.h"
#include "tensorflow/lite/c/common.h"
#include "tensorflow/lite/micro/tflite_bridge/micro_error_reporter.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "micro_ops.h"

// Model: models/mobilenet_frozen_quantized_int8.tflite
tflite::MicroMutableOpResolver<7>* get_micro_op_resolver(tflite::ErrorReporter* error_reporter) {
    auto* resolver = new tflite::MicroMutableOpResolver<7>();

    if (resolver->AddMean() != kTfLiteOk) {
        error_reporter->Report("AddMean failed");
        vTaskDelete(NULL);
    }

    if (resolver->AddDepthwiseConv2D() != kTfLiteOk) {
        error_reporter->Report("AddDepthwiseConv2D failed");
        vTaskDelete(NULL);
    }

    if (resolver->AddConv2D() != kTfLiteOk) {
        error_reporter->Report("AddConv2D failed");
        vTaskDelete(NULL);
    }

    if (resolver->AddSoftmax() != kTfLiteOk) {
        error_reporter->Report("AddSoftmax failed");
        vTaskDelete(NULL);
    }

    if (resolver->AddQuantize() != kTfLiteOk) {
        error_reporter->Report("AddQuantize failed");
        vTaskDelete(NULL);
    }

    if (resolver->AddFullyConnected() != kTfLiteOk) {
        error_reporter->Report("AddFullyConnected failed");
        vTaskDelete(NULL);
    }

    if (resolver->AddDequantize() != kTfLiteOk) {
        error_reporter->Report("AddDequantize failed");
        vTaskDelete(NULL);
    }

    return resolver;
}