# ESP TFLITE deployment 

**Table of Contents:**

1. [Introduction](#introduction)
2. [Build and Deploy](#build-and-deploy)
---

## Introduction

This repository is meant to be used as a template for deploying LiteRT (previously TFLite) models on ESP32 microcontrollers.
The current repository was tested by using `esp-idf` version **5.5** and should be able to accept any model that fits inside
the memory and has a single input and a single output tensor.

## Build and Deploy

1. Download the repository along with its dependencies
	``` bash
	git clone --recursive  https://github.com/nubificus/fmnist-esp-ota.git
	cd fmnist-esp-ota
	```

2. Define the environment variables necessary for the project. These are the following:

	* **FIRMWARE_VERSION**: the version of that app used to distinguish it from others.
	* **DEVICE_TYPE**: the type of esp32 device that this project will be compiled for (it should be the same as the one used in `idf.py set-target`).
	* **APPLICATION_TYPE**: the kind of application that will be compiled. In our case it should be named after the tflite model type used.
	* **MODEL_FILE**: this is the path to the tflite model of choice
	* **PORT**: the port to which the esp32 device is connected
	* **TENSOR_ALLOCATION_SPACE**: the size of space that should be allocated (in internal RAM / external PSRAM) for storing the model's tensors.
	* **LOAD_MODEL_FROM_PARTITION**: defined when the tflite model should be read from a flash partition. Otherwise, the model is extracted from a C array found in the `micro_model.cpp` file.
	* **INTERNAL_MEMORY_USAGE**: defined when the space for the tensors should be allocated from the internal RAM. Otherwise, the larger but slower external PSRAM is used.
	* **WIFI_SSID**: the WiFi SSID that the esp32 device should connect to.
	* **WIFI_PASS**: the WiFi password of the defined WiFi SSID
	* **STOCK**: defined when the app does not need OTA update support
	* **OTA_SECURE**: defined to enable secure OTA update support. **STOCK** should not be set along with this option.

	Assuming we aim to deploy the custom *mobilenet* tflite model inside the `models` directory from a flash partition by using the external PSRAM for tensor allocation and enabling non-secure OTA update support, we should do the following:

	```bash
	export FIRMWARE_VERSION="0.1.0"
	export DEVICE_TYPE="esp32s3"
	export APPLICATION_TYPE="mobilenet"
	export MODEL_FILE="models/mobilenet_frozen_quantized_int8.tflite"
	export PORT="dev/ttyUSB2"
	# This is found by trial and error(e.g. mobilenet needs about 302KB)
	export TENSOR_ALLOCATION_SPACE=$((400 * 1024))
	# The tflite model should be expected in a certain partition
	export LOAD_MODEL_FROM_PARTITION=1
	export WIFI_SSID=<wifi_ssid>
	export WIFI_PASS=<wifi_pass>
	```

3. Set the target device, build and flash the project

	```bash
	mkdir build
	idf.py set-target <DEVICE_TYPE>
	idf.py build
	idf.py --port <PORT> flash
	```

	During the building process, the `scripts/esp32_model_deploy_helper.py` script is called which essentially does all the model-specific configurations in order for our tflite deployment app to work. It does the following:
	* Checks if you have exported the **LOAD_MODEL_FROM_PARTITION** variable and creates the `scripts/env.sh` script which defines the **TFLITE_MODEL_SIZE** variable. In that case, it also extends the `tflite_model` partition in the `partitions.csv` file if necessary and writes the model on the esp32 device's flash memory using the esptool.
	* If **LOAD_MODEL_FROM_PARTITION** is not set, then the python script creates the `src/micro_model.cpp` so that the building process can bind the model into the final executable.
	* It finds the micro operations used by the model and creates the `main/inc/micro_ops.h` and `main/src/micro_ops.cpp` which define the `get_micro_op_resolver()` function that is needed by the `setup()` function.

4. Finally, run the application with `idf.py --port <PORT> monitor` and the logging should be similar to the following:
	```bash
	I (2150) wifi: STA IP: 192.168.11.57
	I (2150) wifi: Connected to ap
	I (2150) main: HTTP Server started
	I (2150) main: OTA Handler set
	I (2150) main: Info handler set
	I (2160) allocate_tensor_arena: PSRAM is available! Total size: 2097152 bytes
	I (2160) allocate_tensor_arena: Tensor arena allocated in PSRAM (409600 bytes)
	I (2670) load_model_from_partition: Model successfully mapped from flash
	I (2710) setup: Used tensor arena: 308936 bytes
	I (2710) setup: Performing warmup runs...
	I (13930) setup: Completed 10 warmup runs.
	I (13930) tcp_server: Server is listening on port 1234
	I (13930) tcp_server: Waiting for client connection...
	```

When **INTERNAL_MEMORY_USAGE** is **not defined**, but your device does not provide PSRAM support, you should define it and remove every *CONFIG_SPIRAM* option from `sdkconfig.defaults` before building the app. If you have already built the app, remove everything and start from scratch as shown below:

```bash
rm -rf build
rm -f sdkconfig
mkdir build
idf.py set-target <DEVICE_TYPE>
idf.py build
idf.py --port <PORT> flash monitor
```

If you want to change an environment variable controlling the project, ideally you should do `idf.py fullclean` and then rebuild the project.