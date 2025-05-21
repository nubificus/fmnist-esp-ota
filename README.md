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

	* `DEVICE_TYPE`: the type of esp32 device that this project will be compiled for (it should be the same as the one used in `idf.py set-target`).
	* `WIFI_SSID`: the WiFi SSID that the esp32 device should connect to.
	* `WIFI_PASS`: the WiFi password of the defined WiFi SSID
	* `version`: the version of that app used to distinguish it from others.
	* `type`: the kind of application that will be compiled. In our case it should be named after the tflite model type used.
	* `model`: this is the path to the tflite model of choice
	* `tensor_allocation_space`: the size of space that should be allocated (in internal RAM / external PSRAM) for storing the model's tensors.
	* `load_model_from_partition`: defined when the tflite model should be read from a flash partition. Otherwise, the model is extracted from a C array found in the `micro_model.cpp` file.
	* `tflite_model_size`: this is the size of the tflite model found in `model` and is defined by the `scripts/prebuild.sh` script.
	* `quad_psram`: defined when the space for the tensors should be allocated from the quad external PSRAM.
	* `oct_psram`: defined when the space for the tensors should be allocated from the octal external PSRAM. If neither `quad_psram` nor `oct_psram` is defined, then the smaller but faster internal RAM is used.
	* `STOCK`: defined when the app does not need OTA update support.
	* `OTA_SECURE`: defined to enable secure OTA update support. `STOCK` should not be set along with this option.

	Assuming we aim to deploy the custom *resnet8* tflite model inside the `models` directory without a flash partition, using the external octal PSRAM for tensor allocation and enabling secure OTA update support, we should do the following:

	```bash
	export DEVICE_TYPE="esp32s3"
	export WIFI_SSID=<wifi_ssid>
	export WIFI_PASS=<wifi_pass>

	export version="1.1.1"
	export type="resnet8"
	export model="models/resnet8_frozen.tflite"
	export tensor_allocation_space=$((200 * 1024))

	export oct_psram=""
	export OTA_SECURE=""
	```

3. Run the prebuild script

	```bash
	. ./scripts/prebuild.sh
	```

	This script does the following:
	* Constructs an appropriate `sdkconfig.defaults` based on the existence of `quad_psram` and `oct_psram`.
	* Defines the `tflite_model_size` environment variable based on the `model` environment variable.
	* Creates a python virtual environment for running the `scripts/tflite_micro_helper.py` script.

	The `scripts/tflite_micro_helper.py` script, which expects the `model` environment variable, is used for:
	* Creating the `src/micro_model.cpp` file if `load_model_from_partition` is not defined.
	* Creating the `src/micro_ops.cpp` and `inc/micro_ops.h` which implement the function `get_micro_op_resolver()`. That function uniquely defines the operations used by the model of choice. In our example the function generated is the following:

		```c++
		tflite::MicroMutableOpResolver<7>* get_micro_op_resolver(tflite::ErrorReporter* error_reporter) {
			auto* resolver = new tflite::MicroMutableOpResolver<7>();

			if (resolver->AddMaxPool2D() != kTfLiteOk) {
				error_reporter->Report("AddMaxPool2D failed");
				vTaskDelete(NULL);
			}

			if (resolver->AddSoftmax() != kTfLiteOk) {
				error_reporter->Report("AddSoftmax failed");
				vTaskDelete(NULL);
			}

			if (resolver->AddMean() != kTfLiteOk) {
				error_reporter->Report("AddMean failed");
				vTaskDelete(NULL);
			}

			if (resolver->AddMul() != kTfLiteOk) {
				error_reporter->Report("AddMul failed");
				vTaskDelete(NULL);
			}

			if (resolver->AddFullyConnected() != kTfLiteOk) {
				error_reporter->Report("AddFullyConnected failed");
				vTaskDelete(NULL);
			}

			if (resolver->AddConv2D() != kTfLiteOk) {
				error_reporter->Report("AddConv2D failed");
				vTaskDelete(NULL);
			}

			if (resolver->AddAdd() != kTfLiteOk) {
				error_reporter->Report("AddAdd failed");
				vTaskDelete(NULL);
			}

			return resolver;
		}
		```

4. Set the target device and build the project

	```bash
	idf.py set-target <DEVICE_TYPE>
	idf.py build
	```

5. Use the firmware assembler to optionally sign the app and finally bundle the firmware:

	In our example, we want to have secure mode enabled so we need to inlcude the **--secure_boot** option
	```bash
	bash scripts/assemble_firmware.sh --secure_boot
	```

	You should expect a firmware directory of the following structure:
	```bash
	firmware/
	├── esp32s3
	│   └── artifacts
	│       ├── bootloader.bin
	│       ├── esp_tflite_app.bin
	│       ├── ota_data_initial.bin
	│       ├── partitions.csv
	│       ├── partition-table.bin
	│       └── sdkconfig
	└── model.bin
	```

	**WARNING**: The `scripts/assemble_firmware.sh` script assumes that the *signing version is 2* and that the key resides in the `./secure_boot_signing_key.pem` file

6. Use the flasher to flash the application on the esp32

	To do that you need to use the `scripts/flasher.sh` script, which offers the option to override the default partition table and build a new one with the `scripts/ptmaker.sh`. You have the following partition table options that are passed from the flasher to the ptmaker:
	* `normal`: creates three app partitions (one factory and two ota).
	* `normal_with_model`: creates three app partitions (one factory and two ota) and a custom tflite_model partition.
	* `no_factory`: creates two app partitions (two ota and the app will be placed in ota_0).
	* `no_factory_with_model`: creates two app partitions (two ota and the app will be placed in ota_0) and a custom tflite_model partition.

	Note that the flash size is also passed as an argument and it is used to expand the partitions based on the hardware capabilities.
	Bearing all that in mind, we should do the following to replace the current partition table with a `normal` one, which will exploit all our device's flash memory, and flash the proper binaries to that device:
	```bash
	bash scripts/flasher.sh --port <PORT> --chip <DEVICE_TYPE> --flash_size <FLASH_SIZE> --override_pt normal
	```

7. Finally, run the application with `idf.py --port <PORT> monitor` and the logging should be similar to the following:
	```bash
	I (2519) wifi: STA IP: 192.168.11.133
	I (2519) wifi: Connected to ap
	I (2519) main: HTTP Server started
	I (2519) main: OTA Handler set
	I (2529) main: Info handler set
	I (2529) allocate_tensor_arena: PSRAM is available! Total size: 8388608 bytes
	I (2539) allocate_tensor_arena: Tensor arena allocated in PSRAM (204800 bytes)
	I (3039) setup: Used tensor arena: 154764 bytes
	I (3039) setup: Performing warmup runs...
	I (21449) [setup]: Completed 10 warmup runs.
	I (21449) [tcp_server]: Server is listening on port 1234
	I (21449) tcp_server: Waiting for client connection...
	```