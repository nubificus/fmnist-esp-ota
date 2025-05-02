cmake_minimum_required(VERSION 3.5)
set(CMAKE_CXX_STANDARD 17)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# Allow user to specify the model file and port for the python script
if(NOT DEFINED MODEL_FILE)
	set(MODEL_FILE "models/mobilenet_frozen_quantized_int8.tflite")
	message(WARNING "MODEL_FILE is not set, using default: ${MODEL_FILE}")
endif()

# if(NOT DEFINED PORT)
# 	set(PORT "/dev/ttyUSB2")
# 	message(WARNING "PORT is not set, using default: ${PORT}")
# endif()

# Run the Python script
execute_process(
	COMMAND python3 scripts/esp32_model_deploy_helper.py ${MODEL_FILE} #--port ${PORT}
	WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
	RESULT_VARIABLE SCRIPT_RESULT
)
if(NOT SCRIPT_RESULT EQUAL 0)
	message(FATAL_ERROR "Failed to execute esp32_model_deploy_helper.py")
endif()

# Set default values for environment variables
if(DEFINED ENV{FIRMWARE_VERSION})
	add_compile_definitions(FIRMWARE_VERSION=\"$ENV{FIRMWARE_VERSION}\")
else()
	add_compile_definitions(FIRMWARE_VERSION="1.1.1")
	message(WARNING "FIRMWARE_VERSION is not set, using default value")
endif()

if(DEFINED ENV{DEVICE_TYPE})
	add_compile_definitions(DEVICE_TYPE=\"$ENV{DEVICE_TYPE}\")
else()
	add_compile_definitions(DEVICE_TYPE="esp32s3")
	message(WARNING "DEVICE_TYPE is not set, using default value")
endif()

if(DEFINED ENV{APPLICATION_TYPE})
	add_compile_definitions(APPLICATION_TYPE=\"$ENV{APPLICATION_TYPE}\")
else()
	add_compile_definitions(APPLICATION_TYPE="simple_cnn")
	message(WARNING "APPLICATION_TYPE is not set, using default value")
endif()

if (DEFINED ENV{TENSOR_ALLOCATION_SPACE})
	math(EXPR TENSOR_ALLOCATION_SPACE_VALUE "$ENV{TENSOR_ALLOCATION_SPACE}")
	add_compile_definitions(TENSOR_ALLOCATION_SPACE=${TENSOR_ALLOCATION_SPACE_VALUE})
	message("TENSOR_ALLOCATION_SPACE is set to ${TENSOR_ALLOCATION_SPACE_VALUE}")
else()
	math(EXPR TENSOR_ALLOCATION_SPACE_VALUE "200 * 1024")
	add_compile_definitions(TENSOR_ALLOCATION_SPACE=${TENSOR_ALLOCATION_SPACE_VALUE})
	message(WARNING "TENSOR_ALLOCATION_SPACE is not set, using default value 200KB")
endif()

if(DEFINED ENV{LOAD_MODEL_FROM_PARTITION})
	add_compile_definitions(LOAD_MODEL_FROM_PARTITION)
	message("Loading model from flash partition")

	# Source the env.sh script to load TFLITE_MODEL_SIZE
	execute_process(
		COMMAND bash -c "source ${CMAKE_SOURCE_DIR}/scripts/env.sh && echo \$TFLITE_MODEL_SIZE"
		OUTPUT_VARIABLE TFLITE_MODEL_SIZE_OUTPUT
		OUTPUT_STRIP_TRAILING_WHITESPACE
	)

	# When loading the model from a partition, we need to know the size of the model
	if(NOT "${TFLITE_MODEL_SIZE_OUTPUT}" STREQUAL "")
		math(EXPR TFLITE_MODEL_SIZE_VALUE "${TFLITE_MODEL_SIZE_OUTPUT}")
		add_compile_definitions(TFLITE_MODEL_SIZE=${TFLITE_MODEL_SIZE_VALUE})
		message("TFLITE_MODEL_SIZE is set to ${TFLITE_MODEL_SIZE_VALUE}")
	else()
		message(FATAL_ERROR "TFLITE_MODEL_SIZE is not set in env.sh")
	endif()
else()
	message(WARNING "Loading model from the micro_model.cpp file")
endif()

if(NOT DEFINED ENV{INTERNAL_MEMORY_USAGE})
	add_compile_definitions(INTERNAL_MEMORY_USAGE)
	message("Using internal RAM configuration")
	execute_process(
		COMMAND bash -c "cp ${CMAKE_SOURCE_DIR}/sdkconfig.defaults.internal ${CMAKE_SOURCE_DIR}/sdkconfig.defaults"
		RESULT_VARIABLE COPY_RESULT
	)
	if(NOT COPY_RESULT EQUAL 0)
		message(FATAL_ERROR "Failed to copy sdkconfig.defaults.internal to sdkconfig.defaults")
	endif()
else() # Enable external PSRAM
	message("Using PSRAM configuration")
	execute_process(
		COMMAND bash -c "cp ${CMAKE_SOURCE_DIR}/sdkconfig.defaults.psram ${CMAKE_SOURCE_DIR}/sdkconfig.defaults"
		RESULT_VARIABLE COPY_RESULT
	)
	if(NOT COPY_RESULT EQUAL 0)
		message(FATAL_ERROR "Failed to copy sdkconfig.defaults.psram to sdkconfig.defaults")
	endif()
endif()

if(DEFINED ENV{WIFI_SSID})
	add_compile_definitions(WIFI_SSID=\"$ENV{WIFI_SSID}\")
else()
	add_compile_definitions(WIFI_SSID="guest")
	message(WARNING "WIFI_SSID is not set, using default value")
endif()

if(DEFINED ENV{WIFI_PASS})
	add_compile_definitions(WIFI_PASS=\"$ENV{WIFI_PASS}\")
else()
	add_compile_definitions(WIFI_PASS="guest")
	message(WARNING "WIFI_PASS is not set, using default value")
endif()

# Conditionally define the STOCK and OTA_SECURE macros
if(DEFINED ENV{STOCK})
	add_compile_definitions(STOCK)
	message("OTA update disabled")
else()
	message(WARNING "OTA update enabled")
endif()

if(DEFINED ENV{OTA_SECURE})
	add_compile_definitions(OTA_SECURE)
	message("Secure OTA update enabled")
else()
	message(WARNING "Secure OTA update disabled")
endif()

project(esp_tflite_app)