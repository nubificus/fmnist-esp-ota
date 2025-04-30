#pragma once

#include "tensorflow/lite/micro/micro_mutable_op_resolver.h"
#include "tensorflow/lite/micro/tflite_bridge/micro_error_reporter.h"

tflite::MicroMutableOpResolver<7>* get_micro_op_resolver(tflite::ErrorReporter* error_reporter);