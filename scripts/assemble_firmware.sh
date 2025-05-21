#!/bin/bash

# Configuration for secure boot signing
SECURE_BOOT_VERSION="2"
SECURE_BOOT_KEYFILE="secure_boot_signing_key.pem"

BUILD_DIR="./build"
APP_BINARY=$(ls $BUILD_DIR | grep -E '\.bin$' | grep -v -e ota_data)

# Check if the app binary is found
if [ -z "$APP_BINARY" ]; then
	echo "Error: Could not locate the app binary in $BUILD_DIR."
	exit 1
fi

APP_BINARY_UNSIGNED="$BUILD_DIR/${APP_BINARY}-unsigned"
APP_BINARY="$BUILD_DIR/$APP_BINARY"

# Check if the script is being run with the --secure_boot argument
if [ "$1" == "--secure_boot" ]; then
	echo "Secure boot enabled."

	# Define paths for unsigned bootloader and app binary
	BOOTLOADER="$BUILD_DIR/bootloader/bootloader.bin"
	BOOTLOADER_UNSIGNED="$BUILD_DIR/bootloader/bootloader.bin-unsigned"

	# Check if unsigned bootloader exists
	if [ ! -f "$BOOTLOADER_UNSIGNED" ]; then
		echo "Signing bootloader..."
		mv "$BOOTLOADER" "$BOOTLOADER_UNSIGNED"
		espsecure.py sign_data --version "$SECURE_BOOT_VERSION" --keyfile "$SECURE_BOOT_KEYFILE" \
			--output "$BOOTLOADER" "$BOOTLOADER_UNSIGNED"
	fi

	# Check if unsigned app binary exists
	if [ ! -f "$APP_BINARY_UNSIGNED" ]; then
		echo "Signing app binary..."
		mv "$APP_BINARY" "$APP_BINARY_UNSIGNED"
		espsecure.py sign_data --version "$SECURE_BOOT_VERSION" --keyfile "$SECURE_BOOT_KEYFILE" \
			--output "$APP_BINARY" "$APP_BINARY_UNSIGNED"
	fi
fi

# Check if DEVICE_TYPE environment variable is set
if [ -z "$DEVICE_TYPE" ]; then
	echo "Error: DEVICE_TYPE environment variable is not set."
	exit 1
fi

# Check if model environment variable is set and points to a valid file
if [ -z "$model" ] || [ ! -f "$model" ]; then
	echo "Error: 'model' environment variable is not set or the file does not exist."
	exit 1
fi

# Define paths
FIRMWARE_DIR="./firmware"
DEVICE_DIR="$FIRMWARE_DIR/$DEVICE_TYPE"
ARTIFACTS_DIR="$DEVICE_DIR/artifacts"

# Create the firmware directory structure
echo "Creating firmware directory structure..."
mkdir -p "$ARTIFACTS_DIR"

# Copy the model file
echo "Copying model file to $FIRMWARE_DIR/model.bin..."
cp "$model" "$FIRMWARE_DIR/model.bin"

# Copy required files from the build directory to the artifacts directory
SDKCONFIG="./sdkconfig"
PARTITION_CSV="./$(grep -E '^CONFIG_PARTITION_TABLE_FILENAME=' "${SDKCONFIG}" | cut -d'=' -f2 | sed 's/^"//; s/"$//')"
BOOTPATH="$BUILD_DIR/bootloader/bootloader.bin"
TABLEPATH="$BUILD_DIR/partition_table/partition-table.bin"
OTAPATH="$BUILD_DIR/ota_data_initial.bin"
IMGPATH="$APP_BINARY"

REQUIRED_FILES=("$SDKCONFIG" "$PARTITION_CSV" "$BOOTPATH" "$TABLEPATH" "$OTAPATH" "$IMGPATH")
for file in "${REQUIRED_FILES[@]}"; do
	if [ ! -f "$file" ]; then
		echo "Error: Required file '$file' not found."
		exit 1
	fi
	echo "Copying $file to $ARTIFACTS_DIR..."
	cp "$file" "$ARTIFACTS_DIR/"
done

echo "Firmware assembly completed successfully."