#!/bin/bash

# Exit immediately if a command exits with a non-zero status
set -e

# Define the venv directory
VENV_DIR=".venv"

# Create the virtual environment
python3 -m venv $VENV_DIR

# Activate the virtual environment
source $VENV_DIR/bin/activate

# Upgrade pip and install requirements
pip install --upgrade pip
pip install requests
pip install ai-edge-litert

# Run the Python script with the virtual environment
python3 "$@"

# Deactivate the virtual environment
deactivate

# Remove the virtual environment
rm -rf $VENV_DIR