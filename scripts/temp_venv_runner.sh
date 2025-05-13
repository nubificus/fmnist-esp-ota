#!/bin/bash

# Exit immediately if a command exits with a non-zero status
set -e


echo "Running setup_env.sh"
SCRIPT_DIR=$(dirname "$(realpath "$0")")

# Delete this so that it can be recreated during build if it exists
echo "Trying to find $SCRIPT_DIR/sdkconfig file."
if [ -f $SCRIPT_DIR/sdkconfig ]; then
	rm $SCRIPT_DIR/sdkconfig
	echo "Deleted $SCRIPT_DIR/sdkconfig file."
else
	echo "$SCRIPT_DIR/sdkconfig file not found, nothing to delete."
fi

# Create a virtual environment for running the tflite_micro_helper.py script
VENV_DIR=".venv"
python3 -m venv $VENV_DIR
echo "Virtual environment created at $VENV_DIR"

# Activate the virtual environment
source $VENV_DIR/bin/activate

# Upgrade pip and install requirements
pip install requests
pip install ai-edge-litert

# Run the Python script and capture its stdout and stderr
echo "Running Python script: $@"

if python3 "$@"; then
	echo "Python script executed successfully."
else
	echo "Python script failed with an error."
fi

# Deactivate and remove the virtual environment
deactivate
rm -rf $VENV_DIR
echo "Virtual environment deleted."