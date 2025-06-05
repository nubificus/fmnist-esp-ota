#!/bin/sh

# Step 1: Remove sdkconfig and sdkconfig.defaults
rm -rf sdkconfig sdkconfig.defaults
echo "Removed sdkconfig and sdkconfig.defaults."

# Step 2: Create a new sdkconfig.defaults with base configurations
cat <<EOF > sdkconfig.defaults
CONFIG_ESPTOOLPY_FLASHSIZE_8MB=y
CONFIG_ESPTOOLPY_HEADER_FLASHSIZE_UPDATE=y

CONFIG_PARTITION_TABLE_CUSTOM=y
CONFIG_PARTITION_TABLE_CUSTOM_FILENAME="partitions.csv"
CONFIG_PARTITION_TABLE_OFFSET=0x10000

CONFIG_ESP_IMAGE_BOOTLOADER_SEL_OTA=y
CONFIG_ESP_WIFI_GMAC_SUPPORT=n
CONFIG_PARTITION_TABLE_MD5=y
CONFIG_MBEDTLS_HKDF_C=y
EOF
echo "Created new sdkconfig.defaults"

# Step 3: Add PSRAM configurations if requested
if [ -n "${quad_psram+x}" ]; then
	echo "Adding QUAD PSRAM configurations..."
	cat <<EOF >> sdkconfig.defaults

CONFIG_SPIRAM=y
CONFIG_SPIRAM_MODE_QUAD=y
CONFIG_SPIRAM_TYPE_AUTO=y
CONFIG_SPIRAM_SPEED_40M=y
CONFIG_SPIRAM_SPEED=40
CONFIG_SPIRAM_BOOT_INIT=y
CONFIG_SPIRAM_USE_MALLOC=y
CONFIG_SPIRAM_MEMTEST=y
CONFIG_SPIRAM_MALLOC_ALWAYSINTERNAL=16384
CONFIG_SPIRAM_MALLOC_RESERVE_INTERNAL=32768
EOF
elif [ -n "${oct_psram+x}" ]; then
	echo "Adding OCT PSRAM configurations..."
	cat <<EOF >> sdkconfig.defaults

CONFIG_SPIRAM=y
CONFIG_SPIRAM_MODE_OCT=y
CONFIG_SPIRAM_TYPE_AUTO=y
CONFIG_SPIRAM_SPEED_40M=y
CONFIG_SPIRAM_SPEED=40
CONFIG_SPIRAM_BOOT_INIT=y
CONFIG_SPIRAM_USE_MALLOC=y
CONFIG_SPIRAM_MEMTEST=y
CONFIG_SPIRAM_MALLOC_ALWAYSINTERNAL=16384
CONFIG_SPIRAM_MALLOC_RESERVE_INTERNAL=32768
EOF
else
	echo "No PSRAM configuration added."
fi

# Step 4: Define tflite_model_size based on the model file
if [ -z "$model" ] || [ ! -f "$model" ]; then
	echo "Error: 'model' environment variable is not set or the file does not exist."
	exit 1
fi

len=$(ls -l "$model" | awk '{ print $5 }')
export tflite_model_size=$len
echo "tflite_model_size is set to $tflite_model_size."

# Step 5: Create a virtual environment and run the Python script
VENV_DIR=".venv"
python3 -m venv $VENV_DIR
echo "Virtual environment created at $VENV_DIR"
. ./$VENV_DIR/bin/activate
pip install requests ai-edge-litert

echo "Running tflite_micro_helper.py with model: $model..."
python3 scripts/tflite_micro_helper.py "$model"
if [ $? -ne 0 ]; then
	echo "Python script failed with an error."
	deactivate
	rm -rf $VENV_DIR
	echo "Virtual environment deleted."
	exit 1
else
	echo "Python script executed successfully."
fi

deactivate
rm -rf $VENV_DIR
echo "Virtual environment deleted."
echo "Prebuild script completed."