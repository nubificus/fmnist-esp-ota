# ESP TFLITE deployment 

**Table of Contents:**

1. [Introduction](#introduction)
2. [Build and Deploy](#build-and-deploy)
3. [Input Data TCP Client](#input-data-tcp-client)
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
	git checkout feat_psram
	```

2. Define the environment variables necessary for the project. These are the following:

	* **FIRMWARE_VERSION**: the version of that app used to distinguish it from others.
	* **DEVICE_TYPE**: the type of esp32 device that this project will be compiled for (it should be the same as the one used in `idf.py set-target`).
	* **APPLICATION_TYPE**: the kind of application that will be compiled. In our case it should be named after the tflite model type used.
	* **TFLITE_MODEL_SIZE**: used when the model is loaded from a partition to define the size in bytes of the .tflite model file.
	* **TENSOR_ALLOCATION_SPACE**: the size of space that should be allocated (in internal RAM / external PSRAM) for storing the model's tensors.
	* **LOAD_MODEL_FROM_PARTITION**: defined when the tflite model should be read from a flash partition. Otherwise, the model is extracted from a C array found in the `micro_model.cpp` file.
	* **INTERNAL_MEMORY_USAGE**: defined when the space for the tensors should be allocated from the internal RAM. Otherwise, the larger but slower external PSRAM is used.
	* **WIFI_SSID**: the WiFi SSID that the esp32 device should connect to.
	* **WIFI_PASS**: the WiFi password of the defined WiFi SSID
	* **STOCK**: defined when the app does not need OTA update support
	* **OTA_SECURE**: defined to enable secure OTA update support. **STOCK** should not be set along with this option.

	Assuming we aim to deploy the custom *mobilenet* tflite model inside the `models` directory from a flash partition by using the external PSRAM for tensor allocation and enabling non-secure OTA update support, we should do the following:

	```bash
	stat -c%s models/mobilenet_frozen_quantized_int8.tflite
	```
	This should give the size of the tflite file in bytes (i.e. **1181664** bytes for this mobilenet)

	```bash
	export FIRMWARE_VERSION="0.1.0"
	export DEVICE_TYPE="esp32s3"
	export APPLICATION_TYPE="mobilenet"
	# The size extracted previously
	export TFLITE_MODEL_SIZE=1181664
	# This is found by trial and error(e.g. mobilenet needs about 302KB)
	export TENSOR_ALLOCATION_SPACE=$((400 * 1024))
	# The tflite model should be expected in a certain partition
	export LOAD_MODEL_FROM_PARTITION=1
	export WIFI_SSID=<wifi_ssid>
	export WIFI_PASS=<wifi_pass>
	```

3. When **LOAD_MODEL_FROM_PARTITION** is **not defined**, you need to produce a .cpp file from the model's .tflite file and copy it onto the `main/src/micro_model.cpp` file. Therefore, do:
	```bash
	xxd -i models/<micro_model>.tflite > main/src/micro_model.cpp
	```
	Then, edit the .cpp file to ensure its form is:
	```cpp
	#include "micro_model.h"
	const unsigned char micro_model_cc_data[] = {\*Binary data*\}
	const unsigned int micro_model_cc_data_len
	```

	Note that the *tflite_model* partition in the `partitions.csv` file can be removed when loading the model from `main/src/micro_model.cpp` since it would otherwise become unused space.

4. Finally set the target device, build and flash the project

	```bash
	mkdir build
	idf.py set-target <DEVICE_TYPE>
	idf.py build
	idf.py --port <PORT> flash
	```

5. In case **LOAD_MODEL_FROM_PARTITION** is **defined**, ensure the tflite model is written in the appropriate *tflite_model* partition which is defined in the following `partitions.csv` file:

	```
	# Name,       Type, SubType, Offset,   Size,    Flags
	nvs,          data, nvs,     0x11000,  0x6000,
	phy_init,     data, phy,     0x17000,  0x1000,
	factory,      app,  factory, 0x20000,  0x180000,
	ota_0,        app,  ota_0,   0x1A0000, 0x180000,
	ota_1,        app,  ota_1,   0x320000, 0x180000,
	otadata,      data, ota,     0x4A0000, 0x2000,
	tflite_model, data, spiffs,  0x4A2000, 0x180000,
	```
	That can be done with the command:

	```bash
	esptool.py --port /dev/ttyUSB2 write_flash 0x4A2000 models/mobilenet_frozen_quantized_int8.tflite
	```

6. Last but not least, run the application with `idf.py --port <PORT> monitor` and the logging should be similar to the following:
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

## Input Data TCP Client