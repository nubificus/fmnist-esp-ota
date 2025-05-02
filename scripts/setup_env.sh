#!/bin/bash

# Exit immediately if a command exits with a non-zero status
set -e

# Debugging: Print the current environment and working directory
echo "Running setup_env.sh"
echo "Current working directory: $(pwd)"

# Delete this so that it can be recreated during build if it exists
if [ -f $(pwd)/sdkconfig ]; then
	echo "Deleting existing sdkconfig file"
	rm $(pwd)/sdkconfig
fi

# Define the venv directory
VENV_DIR=".venv"

# Create the virtual environment
python3 -m venv $VENV_DIR

# Print a message indicating the venv was created
echo "Virtual environment created at $VENV_DIR"

# Activate the virtual environment
source $VENV_DIR/bin/activate

# Upgrade pip and install requirements
pip install requests
pip install ai-edge-litert

# Run the Python script and capture its stdout and stderr
echo "Running Python script: $@"
PYTHON_STDOUT=$VENV_DIR/python_stdout.log
PYTHON_STDERR=$VENV_DIR/python_stderr.log

if python3 "$@" >"$PYTHON_STDOUT" 2>"$PYTHON_STDERR"; then
	echo "Python script executed successfully."
	echo "Python script stdout:"
	cat "$PYTHON_STDOUT"
else
	echo "Python script failed with an error."
	echo "Python script stdout:"
	cat "$PYTHON_STDOUT"
	echo "Python script stderr:"
	cat "$PYTHON_STDERR"
fi

# Deactivate the virtual environment
deactivate

# Remove the virtual environment
rm -rf $VENV_DIR
echo "Virtual environment deleted."