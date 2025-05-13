cmake_minimum_required(VERSION 3.5)
set(CMAKE_CXX_STANDARD 17)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# Define valid device types
set(PSRAM_DEVICES "esp32" "esp32s2" "esp32s3")

# When PSRAM is enabled, check the device type supports it
if(DEFINED ENV{enable_psram} AND NOT "$ENV{DEVICE_TYPE}" IN_LIST PSRAM_DEVICES)
	message(FATAL_ERROR "PSRAM is enabled, but DEVICE_TYPE: $ENV{DEVICE_TYPE} is not one of the supported PSRAM devices: ${PSRAM_DEVICES}")
endif()

# General variables
if(DEFINED ENV{DEVICE_TYPE})
	add_compile_definitions(DEVICE_TYPE=\"$ENV{DEVICE_TYPE}\")
else()
	add_compile_definitions(DEVICE_TYPE="esp32s3")
	message(WARNING "DEVICE_TYPE is not set, using default value")
endif()

if(NOT DEFINED ENV{FLASH_SIZE})
	set(FLASH_SIZE "4MB")
	message(WARNING "FLASH_SIZE is not set, using default: ${FLASH_SIZE}")
else()
	set(FLASH_SIZE $ENV{FLASH_SIZE})
	message("FLASH_SIZE is set to: ${FLASH_SIZE}")
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

# App-specific variables
if(DEFINED ENV{version})
	add_compile_definitions(FIRMWARE_VERSION=\"$ENV{version}\")
else()
	add_compile_definitions(FIRMWARE_VERSION="1.1.1")
	message(WARNING "FIRMWARE_VERSION is not set, using default value")
endif()

if(DEFINED ENV{type})
	add_compile_definitions(APPLICATION_TYPE=\"$ENV{type}\")
else()
	add_compile_definitions(APPLICATION_TYPE="simple_cnn")
	message(WARNING "APPLICATION_TYPE is not set, using default value")
endif()

if(NOT DEFINED ENV{model})
	set(MODEL_FILE "models/simple_cnn_tf_frozen.tflite")
	message(WARNING "MODEL_FILE is not set, using default: ${MODEL_FILE}")
else()
	set(MODEL_FILE $ENV{model})
	message("MODEL_FILE is set to: ${MODEL_FILE}")
endif()

if (DEFINED ENV{tensor_allocation_space})
	math(EXPR TENSOR_ALLOCATION_SPACE_VALUE "$ENV{tensor_allocation_space}")
	add_compile_definitions(TENSOR_ALLOCATION_SPACE=${TENSOR_ALLOCATION_SPACE_VALUE})
	message("TENSOR_ALLOCATION_SPACE is set to ${TENSOR_ALLOCATION_SPACE_VALUE}")
else()
	math(EXPR TENSOR_ALLOCATION_SPACE_VALUE "200 * 1024")
	add_compile_definitions(TENSOR_ALLOCATION_SPACE=${TENSOR_ALLOCATION_SPACE_VALUE})
	message(WARNING "TENSOR_ALLOCATION_SPACE is not set, using default value 200KB")
endif()

if(DEFINED ENV{load_model_from_partition})
	add_compile_definitions(LOAD_MODEL_FROM_PARTITION)
	message("Loading model from flash partition")
else()
	message(WARNING "Loading model from the micro_model.cpp file")
endif()

if(DEFINED ENV{enable_psram})
	add_compile_definitions(ENABLE_PSRAM)
	message("Using PSRAM configuration")
	execute_process(
		COMMAND bash -c "cp ${CMAKE_SOURCE_DIR}/sdkconfig.defaults.psram ${CMAKE_SOURCE_DIR}/sdkconfig.defaults"
		RESULT_VARIABLE COPY_RESULT
	)
	if(NOT COPY_RESULT EQUAL 0)
		message(FATAL_ERROR "Failed to copy sdkconfig.defaults.psram to sdkconfig.defaults")
	endif()
else() # Enable internal RAM configuration
	message("Using internal RAM configuration")
	execute_process(
		COMMAND bash -c "cp ${CMAKE_SOURCE_DIR}/sdkconfig.defaults.internal ${CMAKE_SOURCE_DIR}/sdkconfig.defaults"
		RESULT_VARIABLE COPY_RESULT
	)
	if(NOT COPY_RESULT EQUAL 0)
		message(FATAL_ERROR "Failed to copy sdkconfig.defaults.internal to sdkconfig.defaults")
	endif()
endif()

# Run ptmaker.sh to generate the partition table
if(DEFINED ENV{load_model_from_partition})
	message(STATUS "Running ptmaker.sh with model file: ${MODEL_FILE}")
	execute_process(
		COMMAND bash -c "source ${CMAKE_SOURCE_DIR}/scripts/ptmaker.sh ${FLASH_SIZE} ${MODEL_FILE} > ${CMAKE_SOURCE_DIR}/partitions.csv"
		WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
		RESULT_VARIABLE PT_RESULT
	)
else()
	message(STATUS "Running ptmaker.sh without model file")
	execute_process(
		COMMAND bash -c "source ${CMAKE_SOURCE_DIR}/scripts/ptmaker.sh ${FLASH_SIZE} > ${CMAKE_SOURCE_DIR}/partitions.csv"
		WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
		RESULT_VARIABLE PT_RESULT
	)
endif()

if(NOT PT_RESULT EQUAL 0)
	message(FATAL_ERROR "ptmaker.sh failed to generate partitions.csv")
else()
	message(STATUS "Partition table generated with FLASH_SIZE=${FLASH_SIZE}")
endif()

# Use the temp_venv_runner.sh script to run the python script responsible for setting up the model-specific files
execute_process(
	COMMAND bash ${CMAKE_SOURCE_DIR}/scripts/temp_venv_runner.sh ${CMAKE_SOURCE_DIR}/scripts/tflite_micro_helper.py ${MODEL_FILE}
	WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
	RESULT_VARIABLE SCRIPT_RESULT
)
if(NOT SCRIPT_RESULT EQUAL 0)
	message(FATAL_ERROR "Failed to execute tflite_micro_helper.py in virtual environment")
else()
	message(STATUS "tflite_micro_helper.py executed successfully")
endif()

project(esp_tflite_app)