import os
import socket
import struct
import sys

labels = ["Tshirt_top", "Trouser", "Pullover", "Dress", "Coat",
		"Sandal", "Shirt", "Sneaker", "Bag", "Ankle_boot"]
IMAGE_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)), "../test_data")

def load_images(image_dir):
	images = []
	for filename in sorted(os.listdir(image_dir)):
		label_index = -1
		for i, label in enumerate(labels):
			if label in filename:
				label_index = i
				break
		if label_index == -1:
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
	server_ip = '192.168.11.52'  # Replace with the IP address of your ESP32 server
	server_port = 1234
	images = load_images(IMAGE_DIR)
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
			inference_time = struct.unpack('q', inference_time_data)[0]

			# Print the results sorted in descending order
			sorted_scores = sorted(zip(labels, scores), key=lambda x: x[1], reverse=True)
			for label, score in sorted_scores:
				print(f"Label {label}: {score * 100:.4f}%")
			print(f"Inference time: {inference_time / 1000} ms")
			print()

			image_index = (image_index + 1) % image_count

			if image_index == 0 and not log_file.closed:
				print("All images have been sent, switch back to stdout")
				sys.stdout = original_stdout
				log_file.close()

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