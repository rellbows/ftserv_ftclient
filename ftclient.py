# Program Name: ftclient
# Description: Simple file transfer server.
# Author: Ryan Ellis
# Class: CS372 - Intro to Networks
# Citations: Below Python doc examples used as reference is setting up receive
# function.
# https://docs.python.org/2/howto/sockets.html

# TODO: HEADERS ON ALL FUNCTIONS
# TODO: CLEANUP TESTING STATEMENTS

from socket import *
import sys
import struct

# Description: setup connection for sending instructions to server
# Argument(s): server name and port number it is listening
# Returns: socket connection
def control_setup(a_server_name, a_port_num):
	server_name = a_server_name
	server_port = a_port_num
	init_client_socket = socket(AF_INET, SOCK_STREAM)
	init_client_socket.connect((server_name, server_port))
	return init_client_socket

# Description: setups connection from server to client to send data back
# Argument(s): port number for client to listen on
# Returns: a socket that is ready to receive incoming messages
def data_setup(a_port_num):
	server_port = a_port_num
	server_name = gethostbyname(gethostname())
	init_server_socket = socket(AF_INET, SOCK_STREAM)
	init_server_socket.setsockopt(SOL_SOCKET, SO_REUSEADDR, 1)
	init_server_socket.bind((server_name, server_port))
	return init_server_socket

# Description: sends instructions to server about file transfer
# Arguments: the file transfer option, the necessary info for connections, and connection
# Returns: nothing
def send_buffer(client_name, ft_option, req_filename, data_port, control_connection):
	if(ft_option == '-l'):
		raw_buffer = client_name + ' ' + ft_option + ' ' + str(data_port)
	elif(ft_option == '-g'):
		raw_buffer = client_name + ' ' + ft_option + ' ' + req_filename + ' ' + str(data_port)
	send_buffer = bytes(raw_buffer, 'utf-8')
	control_connection.send(send_buffer)


# Description: receive message from server
# Arugment(s): socket connection
# Returns: the buffer
# Citation: used below stack thread as reference for previxing packets with msg size
# https://stackoverflow.com/questions/17667903/python-socket-receive-large-amount-of-data
def receive_buffer(a_socket):
	msg_len = receive_buffer_size(a_socket)
	if not msg_len:
		return None

	return receive_buffer_helper(a_socket, int(msg_len))

# Description: helps receive buffer from server in chunks
# Argument(s): socket connection
# Returns: byte version of buffer
def receive_buffer_helper(a_socket, num_bytes):
	raw_buffer = b''
	while len(raw_buffer) < num_bytes:
		chunk = a_socket.recv(num_bytes - len(raw_buffer))

		if not chunk:
			return None
		raw_buffer += chunk
	return raw_buffer

# Description: helps get size of received buffer by using size prepended
# to front of buffer + space that delimits where buffer begins
# Argument(s): socket connection
# Returns: size of buffer
def receive_buffer_size(a_socket):
	raw_buffer = str()
	raw_test_buffer = b''
	test_buffer = ''

	# recive bytes until delimiting space
	while test_buffer is not ' ':
		raw_test_buffer = a_socket.recv(1)
		test_buffer = raw_test_buffer.decode('utf-8')
		raw_buffer += test_buffer

	return raw_buffer

# Description: creates file with buffer received from server
# Argument(s): filename requested, and buffer from server
# Returns: nothing
def create_file(req_filename, data_buffer):

	# create file object to write buffer to
	output_fstream = open(req_filename, 'w')

	output_fstream.write(data_buffer)

# Description: shuts down open connection
# Argument(s): socket
# Returns: nothing
def shut_down(a_connection_socket):
	a_connection_socket.close()


def main():
	server_name = str(sys.argv[1])
	server_control_port = int(sys.argv[2])
	ft_option = str(sys.argv[3])
	if(ft_option == "-l"):
		req_filename = None
		client_data_port = int(sys.argv[4])
	elif(ft_option == "-g"):
		req_filename = str(sys.argv[4])
		client_data_port = int(sys.argv[5])
	else:
		print('Invalid option entered! Only \'-l\' (for listing directory)\n and \'-g\' (for getting file) options supported.')
		sys.exit(0)

	# start up connection for control
	control_socket = control_setup(server_name, server_control_port)
	
	# send buffer to server
	send_buffer(gethostname(), ft_option, req_filename, client_data_port, control_socket);

	# shut down the control connection
	control_socket.close()

	# open up data connection
	data_socket = data_setup(client_data_port)

	# data socket once connection made
	new_data_socket = None

	# listen for incoming traffic/data
	# Citation: used below stack overflow thread as reference for handling
	# termination of while loop
	# https://stackoverflow.com/questions/18994912/ending-an-infinite-while-loop
	while(new_data_socket == None):
		data_socket.listen(1)
		new_data_socket, addr = data_socket.accept()

	# don't need parent data conection
	shut_down(data_socket)

	# get buffer from server
	raw_data_buffer = receive_buffer(new_data_socket)
	data_buffer = raw_data_buffer.decode('utf-8')

	# don't need child data connection
	shut_down(new_data_socket)

	# deal with buffer based on ft option
	# LIST OPTION #
	if(ft_option == '-l'):
		print('Receiving directory structure from ' + server_name + ':' + 
			str(client_data_port))
		print(data_buffer)    # print out directory
	# GET OPTION #
	if(ft_option == '-g'):
		print('Receiving \'' + req_filename + '\' from ' + server_name + ':'
			+ str(client_data_port))

		# write buffer to file
		create_file(req_filename, data_buffer)

# main method
if __name__ == '__main__':
	main()	

	