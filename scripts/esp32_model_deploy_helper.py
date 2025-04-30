import argparse
import os
import sys
import stat
import re
import subprocess
import json
import shutil
from ai_edge_litert.interpreter import Interpreter
import requests
import tempfile

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
PARTITIONS_CSV_PATH = os.path.join(SCRIPT_DIR, "../partitions.csv")
TFLITE_MODEL_PARTITION_NAME = "tflite_model"

MICRO_OPS_HEADER_URL = "https://raw.githubusercontent.com/tensorflow/tflite-micro/refs/heads/main/tensorflow/lite/micro/kernels/micro_ops.h"

MICRO_OPS_JSON_PATH = os.path.join(SCRIPT_DIR, "micro_ops.json")
MICRO_OPS_CPP_PATH = os.path.join(SCRIPT_DIR, "../main/src/micro_ops.cpp")
MICRO_OPS_HEADER_PATH = os.path.join(SCRIPT_DIR, "../main/inc/micro_ops.h")

# Returns the offset and size of the tflite_model partition from the partitions.csv file
def parse_partitions_csv():
	with open(PARTITIONS_CSV_PATH, 'r') as f:
		lines = f.readlines()

	offset = None
	size = None
	for line in lines:
		if line.strip().startswith(TFLITE_MODEL_PARTITION_NAME):
			parts = [x.strip() for x in line.strip().split(',')]
			offset = int(parts[3], 16)
			size = int(parts[4], 16)
			return offset, size, lines

	raise RuntimeError("Partition tflite_model not found in CSV")

# Updates the size of the tflite_model partition in the partitions.csv file
def update_partitions_csv(new_size, lines):
	updated = []
	for line in lines:
		if line.strip().startswith(TFLITE_MODEL_PARTITION_NAME):
			parts = [x.strip() for x in line.strip().split(',')]
			parts[4] = f"0x{new_size:X}"
			updated.append(', '.join(parts) + '\n')
		else:
			updated.append(line)
	with open(PARTITIONS_CSV_PATH, 'w') as f:
		f.writelines(updated)

# Writes the model to the specified partition using esptool
def write_model_to_partition(model_path, offset, port):
	subprocess.run([
		"esptool.py", "--port", port, "--no-stub", "write_flash", f"0x{offset:X}", model_path
	], check=True)

# Generates a C array from the model file using xxd
def generate_cpp_array(model_path):
	output_path = "main/src/micro_model.cpp"
	with open(output_path, "w") as out_file:
		subprocess.run(["xxd", "-i", model_path], stdout=out_file, check=True)

	with open(output_path, "r") as f:
		content = f.read()

	content = re.sub(r"unsigned char .*\[]", "const unsigned char micro_model_cc_data[]", content)
	content = re.sub(r"unsigned int .*len", "const unsigned int micro_model_cc_data_len", content)
	content = "#include \"micro_model.h\"\n\n" + content

	with open("main/src/micro_model.cpp", "w") as f:
		f.write(content)

# Finds the operations implemented in TFLite's micro_ops.h file and generates a mapping to the corresponding methods
def parse_micro_ops_header():
	response = requests.get(MICRO_OPS_HEADER_URL)
	response.raise_for_status()
	header_content = response.text

	op_map = {}
	pattern = re.compile(r'TFLMRegistration\*? Register_(\w+)\(\);')
	for line in header_content.splitlines():
		match = pattern.search(line)
		if match:
			op_name = match.group(1)
			method_name = "Add" + ''.join(
				word.capitalize() if not word[0].isdigit() else word[0] + word[1:].capitalize()
				for word in op_name.split('_')
			)
			op_map[op_name.upper()] = method_name

	os.makedirs(os.path.dirname(MICRO_OPS_JSON_PATH), exist_ok=True)
	with open(MICRO_OPS_JSON_PATH, 'w') as f:
		json.dump(op_map, f, indent=4)

	return op_map

# Loads the operations mapping from the JSON file or generates it if it doesn't exist
def load_ops_mapping():
	if os.path.exists(MICRO_OPS_JSON_PATH):
		with open(MICRO_OPS_JSON_PATH, 'r') as f:
			return json.load(f)
	else:
		return parse_micro_ops_header()

# Gets the operations used in the model
def get_model_operations(model_path):
	interpreter = Interpreter(model_path=model_path)
	ops_details = interpreter._get_ops_details()
	return {op['op_name'] for op in ops_details}

# Generate the micro_ops.cpp file, which will contain the get_micro_op_resolver function
def generate_micro_ops_cpp(ops, model_path):
	if os.path.exists(MICRO_OPS_CPP_PATH):
		os.remove(MICRO_OPS_CPP_PATH)

	lines = [
		'#include "tensorflow/lite/micro/micro_mutable_op_resolver.h"',
		'#include "tensorflow/lite/c/common.h"',
		'#include "tensorflow/lite/micro/tflite_bridge/micro_error_reporter.h"',
		'#include "freertos/FreeRTOS.h"',
		'#include "freertos/task.h"',
		'#include "micro_ops.h"',
		'',
		'// Model: {}'.format(model_path),
		'tflite::MicroMutableOpResolver<{}>* get_micro_op_resolver(tflite::ErrorReporter* error_reporter) {{'.format(len(ops)),
		'    auto* resolver = new tflite::MicroMutableOpResolver<{}>();'.format(len(ops)),
		''
	]

	for op in ops:
		lines.append('    if (resolver->{}() != kTfLiteOk) {{'.format(op))
		lines.append('        error_reporter->Report("{} failed");'.format(op))
		lines.append('        vTaskDelete(NULL);')
		lines.append('    }')
		lines.append('')

	lines.append('    return resolver;')
	lines.append('}')

	with open(MICRO_OPS_CPP_PATH, 'w') as f:
		f.write('\n'.join(lines))

# Generate the micro_ops.h file
def generate_micro_ops_header(op_count):
	if os.path.exists(MICRO_OPS_HEADER_PATH):
		os.remove(MICRO_OPS_HEADER_PATH)

	lines = [
		'#pragma once',
		'',
		'#include "tensorflow/lite/micro/micro_mutable_op_resolver.h"',
		'#include "tensorflow/lite/micro/tflite_bridge/micro_error_reporter.h"',
		'',
		f'tflite::MicroMutableOpResolver<{op_count}>* get_micro_op_resolver(tflite::ErrorReporter* error_reporter);'
	]

	with open(MICRO_OPS_HEADER_PATH, 'w') as f:
		f.write('\n'.join(lines))


def main():
	parser = argparse.ArgumentParser()
	parser.add_argument("model_path", help="Path to .tflite model")
	parser.add_argument("--port", help="Serial port for esptool (required if loading from partition)")
	args = parser.parse_args()

	model_path = args.model_path
	port = args.port

	# Check if the model file exists
	if not os.path.exists(model_path):
		print(f"Model file {model_path} does not exist.")
		sys.exit(1)

	load_from_partition = os.getenv("LOAD_MODEL_FROM_PARTITION") == "1"
	if load_from_partition and not port:
		print("Error: --port is required when LOAD_MODEL_FROM_PARTITION=1")
		sys.exit(1)

	# If the user wants to load the model from a partition, we need to check that
	# the partition in the partition table is large enough and then write the model to it.
	# Otherwise, we generate a C array from the model file.
	if load_from_partition:
		model_size = os.stat(model_path).st_size
		env_script_path = os.path.join(SCRIPT_DIR, "env.sh")

		# Create the env.sh script
		with open(env_script_path, "w") as env_script:
			env_script.write(f"# Model: {model_path}\n")
			env_script.write(f"export TFLITE_MODEL_SIZE={model_size}\n")

		print(f"Environment script created: {env_script_path}")
		print(f"Run 'source {env_script_path}' to export TFLITE_MODEL_SIZE={model_size}")

		# Extend the partition size if the model is larger than the partition
		offset, partition_size, csv_lines = parse_partitions_csv()
		if model_size > partition_size:
			update_partitions_csv(model_size, csv_lines)

		# Write the model to the partition
		# write_model_to_partition(model_path, offset, port)
	else:
		# Generate the micro_model.cpp file with the model data
		generate_cpp_array(model_path)

	# Find the model operations
	ops_map = load_ops_mapping()
	model_ops = get_model_operations(model_path)
	unresolved_ops = [op for op in model_ops if op.upper() not in ops_map]
	if unresolved_ops:
		print("Unsupported ops detected:\n" + '\n'.join(model_ops))
		sys.exit(1)

	# Generate the micro_ops.cpp and micro_ops.h files, where the micro_ops.cpp file will contain
	# the get_micro_op_resolver function that will create the resolver with the correct operations.
	mapped_ops = [ops_map[op.upper()] for op in model_ops]
	generate_micro_ops_cpp(mapped_ops, model_path)
	generate_micro_ops_header(len(mapped_ops))

if __name__ == '__main__':
	main()
