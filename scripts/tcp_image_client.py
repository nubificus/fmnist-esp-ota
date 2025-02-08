import os
import socket
import struct
import sys
import argparse

labels = ["T_shirt_top", "Trouser", "Pullover", "Dress", "Coat",
		"Sandal", "Shirt", "Sneaker", "Bag", "Ankle_boot"]

def load_images(image_dir):
	images = []
	for filename in sorted(os.listdir(image_dir)):
		try:
			label_index = labels.index(filename.split('.')[0])
		except ValueError:
			continue  # Skip files that do not match any label
		with open(os.path.join(image_dir, filename), 'rb') as f:
			images.append((label_index, f.read()))
	return images

def recv_all(sock, length):
	data = b''
	while len(data) < length:
		more = sock.recv(length - len(data))
		if not more:
			raise EOFError('Was expecting %d bytes but only received %d bytes before the socket closed' % (length, len(data)))
		data += more
	return data

def main():
	# Parse command line arguments
	parser = argparse.ArgumentParser()
	parser.add_argument("--thres", type=float, default=0.0, help="Confidence threshold for predictions")
	parser.add_argument("--top_k", type=int, default=10, help="Number of top predictions to keep")
	parser.add_argument("--server_ip", type=str, default='192.168.11.57', help="IP address of the ESP32 server")
	parser.add_argument("--server_port", type=int, default=1234, help="Port number of the ESP32 server")
	parser.add_argument("--image_dir", type=str,
						default=os.path.join(os.path.dirname(os.path.abspath(__file__)), "../test_data"),
						help="Directory containing test images")
	args = parser.parse_args()

	# Extract arguments
	thres = args.thres
	top_k = args.top_k
	server_ip = args.server_ip
	server_port = args.server_port
	image_dir = args.image_dir

	images = load_images(image_dir)
	image_count = len(images)

	if image_count == 0:
		print("No images found in the directory")
		return

	print(f"Loaded {image_count} images.")

	client_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
	client_socket.connect((server_ip, server_port))
	print(f"Connected to server at {server_ip} : {server_port}")

	image_index = 0
	log_file = open("results.txt", "w")
	original_stdout = sys.stdout
	sys.stdout = log_file

	# Tracking inference times and accuracy
	correct_predictions = 0
	inference_times = []

	try:
		while True:
			# Send request byte to the server
			client_socket.sendall(b'\x01')

			# Send image data to the server
			label_index, image_data = images[image_index]
			print(f"File: {labels[label_index]}")
			client_socket.sendall(image_data)

			# Receive scores and inference time from the server
			num_labels = len(labels)
			scores_data = recv_all(client_socket, 4 * num_labels)  # num_labels floats, 4 bytes each
			scores = struct.unpack(f'{num_labels}f', scores_data)
			inference_time_data = recv_all(client_socket, 8)  # int64_t, 8 bytes
			inference_time = (struct.unpack('q', inference_time_data)[0]) / 1000
			
			
			# Keep track of correct predictions and inference times
			if not log_file.closed:
				predicted_label = labels[scores.index(max(scores))]
				if predicted_label == labels[label_index]:
					correct_predictions += 1
				inference_times.append(inference_time)

			# Apply threshold and top-K filtering
			filtered_results = [(label, score) for label, score in zip(labels, scores) if score >= thres]
			filtered_results = sorted(filtered_results, key=lambda x: x[1], reverse=True)[:top_k]

			# Print the filtered results
			for label, score in filtered_results:
				print(f"Label {label}: {score * 100:.4f}%")
			print(f"Inference time: {inference_time} ms\n")

			image_index = (image_index + 1) % image_count

			if image_index == 0 and not log_file.closed:
				# Calculate final model accuracy on the given test sample
				if image_count > 0:
					accuracy = (correct_predictions / image_count) * 100
				else:
					accuracy = 0.0

				# Calculate mean and standard deviation of inference times
				if inference_times:
					mean_latency = sum(inference_times) / len(inference_times)
					variance = sum((x - mean_latency) ** 2 for x in inference_times) / len(inference_times)
					std_dev_latency = variance ** 0.5
				else:
					mean_latency = std_dev_latency = 0.0

				# Write final metrics to the log file
				print("Final Metrics:")
				print(f"Accuracy: {accuracy}%")
				print(f"Average Inference Time: {mean_latency} ms")
				print(f"Standard Deviation of Inference Time: {std_dev_latency} ms")
				
				# Switch back to stdout
				sys.stdout = original_stdout
				log_file.close()
				print("All images have been sent, switch back to stdout")

	except Exception as e:
		print(f"Exception: {e}")
	finally:
		print("Closing the client socket")
		client_socket.close()
		if not log_file.closed:
			log_file.close()
		sys.stdout = original_stdout

if __name__ == "__main__":
	main()