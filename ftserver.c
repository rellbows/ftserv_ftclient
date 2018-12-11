/*
** Filename: ftserver.c
** Description: Simple file transfer server.
** Author: Ryan Ellis
** Class: CS372 - Intro to Networks
** Citations: 
** -Most of the boilerplate connection code is based of the 'server.
** c' from 'beejs guide to network programming'
** http://beej.us/guide/bgnet/
** -Used example from GNU's "dirent.h" manual page as reference for getting
** working directory.
** https://www.gnu.org/software/libc/manual/html_node/Simple-Directory-Lister.html
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>
#include <dirent.h>    // for getting working directory
#include <math.h>

#define BACKLOG 10	 // how many pending connections queue will hold
#define MAXFILENAME 255    // max filename size

void sigchld_handler(int s)
{
	(void)s; // quiet unused variable warning

	// waitpid() might overwrite errno, so we save and restore it:
	int saved_errno = errno;

	while(waitpid(-1, NULL, WNOHANG) > 0);

	errno = saved_errno;
}

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
	if (sa->sa_family == AF_INET) {
		return &(((struct sockaddr_in*)sa)->sin_addr);
	}

	return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

// Description: handles all preliminary connection setup of control connection
// Argument(s): string with port
// Returns: integer with sockfd needed for connection
int control_setup(char *serv_port){

	int sockfd;
	struct addrinfo hints, *servinfo, *p;
	struct sigaction sa;
	int yes=1;
	int rv;

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE; // use my IP

	if ((rv = getaddrinfo(NULL, serv_port, &hints, &servinfo)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		return 1;
	}

	// loop through all the results and bind to the first we can
	for(p = servinfo; p != NULL; p = p->ai_next) {
		if ((sockfd = socket(p->ai_family, p->ai_socktype,
				p->ai_protocol)) == -1) {
			perror("server: socket");
			continue;
		}

		if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes,
				sizeof(int)) == -1) {
			perror("setsockopt");
			exit(1);
		}

		if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
			close(sockfd);
			perror("server: bind");
			continue;
		}

		break;
	}

	freeaddrinfo(servinfo); // all done with this structure

	if (p == NULL)  {
		fprintf(stderr, "server: failed to bind\n");
		exit(1);
	}

	if (listen(sockfd, BACKLOG) == -1) {
		perror("listen");
		exit(1);
	}

	sa.sa_handler = sigchld_handler; // reap all dead processes
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART;
	if (sigaction(SIGCHLD, &sa, NULL) == -1) {
		perror("sigaction");
		exit(1);
	}

	return sockfd;
}

// Description: handles all preliminary connection setup of data connection
// Argument(s): string with destination host and port
// Returns: integer with sockfd needed for connection
int data_setup(char *host, char *port){

	// for connection setup
	int sockfd;
	//int numbytes;
	struct addrinfo hints, *servinfo, *p;
	int rv;
	char s[INET6_ADDRSTRLEN];

	memset(&hints, 0, sizeof hints); // clear the contents of the addrinfo struct
	hints.ai_family = AF_UNSPEC; // make the struct IP agnostic
	hints.ai_socktype = SOCK_STREAM; // TCP stream sockets
	hints.ai_flags = AI_PASSIVE; // fill in my IP for me

	// check results of call to getaddrinfo(), if not 0 then error.
	// if 0, then the required structs setup successfully and 
	// returns a pointer to a linked-lists of results

	if((rv = getaddrinfo(host, port, &hints, &servinfo)) != 0){
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		return 1;
	}

	// loop through all the results and connect to the first we can
	for(p = servinfo; p != NULL; p = p->ai_next){
		if((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1){
			perror("client: socket");
			continue;
		}

		if(connect(sockfd, p->ai_addr, p->ai_addrlen) == -1){
			perror("client: connect");
			close(sockfd);
			continue;
		}

		break;
	}

	// checks connections
	if(p == NULL){
		fprintf(stderr, "client: failed to connect\n");
		return 2;
	}

	inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr), s, sizeof s);
	printf("client: connecting to %s\n", s);

	freeaddrinfo(servinfo); // free up memory allocated to servinfo	

	return sockfd;
}

// Description: parse buffer recv from the client to get options/filenames
// as applicable
// Argument(s): pointer to recvd buffer, ft option, data connection 
// name, port , and filename pointers
// Returns: nothing
void parse_buffer(char *buffer, char *client_name,
	char *ft_option, char *req_filename, char *client_port){

	int i = 0;    // iterator to traverse buffer and get data from buffer
	int j = 0;    // iterator to load data into each data field

	// get the client name for the data connection
	while(buffer[i] != ' '){
		client_name[j] = buffer[i];
		i++;
		j++;
	}
	client_name[j] = '\0';

	// move i forward past space
	i++;
	// reset j
	j = 0;

	// get ft option
	while(buffer[i] != ' '){
		ft_option[j] = buffer[i];
		i++;
		j++;
	}
	ft_option[j] = '\0';

	// move i forward past space
	i++;
	// reset j
	j = 0;

	// get requested filename if '-g' option passed
	if(ft_option[0] == '-' && ft_option[1] == 'g'){
		while(buffer[i] != ' '){
			req_filename[j] = buffer[i];
			i++;
			j++;
		}
		req_filename[j] = '\0';

		i++;    // move i forward past space
		j = 0;    // reset j
	}

	// get client port for data connection
	while(buffer[i] != '\0'){
		client_port[j] = buffer[i];
		i++;
		j++;
	}
	client_port[j] = '\0';
}

// Description: gets size of working directory of server
// Argument(s): nothing
// Returns: size of directory
int get_directory_size(){
	DIR *directory;    // holds directory
	struct dirent *file;   // holds file while iterating thru dir
	int directory_size = 0;    // holds directory size
	char *dir_root = ".";
	char *dir_level_up = "..";

	directory = opendir("./");
	if(directory != NULL){
		while((file = readdir(directory))){
			if((strcmp(file->d_name, dir_root)) != 0 && (strcmp(file->d_name, dir_level_up)) != 0){
				directory_size++;
			}
		}
		(void) closedir(directory);
	}
	else{
		perror("Could not get directory size!");
	}

	return directory_size;
}

// Description: gets files in working directory of server
// Argument(s): pointer that will hold filenames in directory
// Returns: nothing
void get_directory(char **directory_array){
	DIR *directory;    // holds directory
	struct dirent *file;   // holds file while iterating thru dir
	char *dir_root = ".";
	char *dir_level_up = "..";
	int i = 0;

	directory = opendir("./");
	if(directory != NULL){
		while((file = readdir(directory))){
			if((strcmp(file->d_name, dir_root)) != 0 && (strcmp(file->d_name, dir_level_up)) != 0){
				strcpy(directory_array[i], file->d_name);

				i++;
			}
		}
		(void) closedir(directory);
	}
	else{
		perror("Could not open directory!");
	}
}

// Description: gets size of the file
// Argument(s): filename of file requested
// Returns: size of the specified file
// Citations: used below stack overflow thread as reference
// https://stackoverflow.com/questions/8236/how-do-you-determine-the-size-of-a-file-in-c
off_t get_file_size(char* req_filename){
	struct stat input_file_stat;    // holds stats on file

	if(stat(req_filename, &input_file_stat) == 0){
		return input_file_stat.st_size;
	}
	return -1;
}

// Description: get file content
// Argument(s): file stream object and char array to hold file contents
// Returns: nothing
// Citation: used below stack overflow thread for reference in how best to 
// grab file contents from file stream
void get_file(FILE *input_fstream, char *input_file, size_t file_size){
	size_t bytes_read = 0;    // holds bytes read by fread()

	// transfer data from stream to c string
	bytes_read = fread(input_file, 1, file_size, input_fstream);
	// check to ensure read correctly
	if(bytes_read != file_size){
		perror("fread");
	}
	// add null terminator
	input_file[file_size] = '\0';

}

// Description: sends the entire buffer
// Argument(s)s: socket fd for data connection, buffer, and total buffer size
// (with buffer length prepended)
// Returns: 0 if send completed, -1 if send failed
// Citations: Used Beej's 'Guide to Network Programming' section on sending
// partial segments of a buffer (Sect. 7.3)
// http://www.beej.us/guide/bgnet/html/multi/advanced.html#sendall
int send_all(int data_sockfd, char *outgoing_buffer, int *total_buffer_size){
	int total = 0;    // bytes sent
	int bytes_left = *total_buffer_size;    // how many bytes left
	int send_status;    // holds status of send (bytes sent when successful and -1 when not)

	while(total < *total_buffer_size){
		send_status = send(data_sockfd, outgoing_buffer + total, bytes_left, 0);
		if(send_status == -1){
			break;
		}
		total += send_status;
		bytes_left -= send_status;
	}

	*total_buffer_size = total;    // returns bytes send using pointer

	return send_status == -1 ? -1:0;    // conditional '?' operator, kind of like if/else
}

// Description: builds buffer to send working directory to client
// Argument(s): socket fd for data connection, array holding filenames and
// number of files in directory
// Returns: nothing
void send_file(int data_sockfd, char *input_file, size_t file_size){
	char *outgoing_buffer = NULL;
	int outgoing_buffer_size = file_size + 1;    // +1 for null terminator
	int outgoing_buffer_size_size = 0;
	int total_buffer_size = 0;    // holds size of buffer + space needed for buffer size prefix
	char *str_outgoing_buffer_size = NULL;

	// get size needed for string holding buffer size
	outgoing_buffer_size_size = snprintf(NULL, 0, "%d", outgoing_buffer_size);
	outgoing_buffer_size_size++;    // add +1 for space delimiting buffer size

	total_buffer_size = outgoing_buffer_size + outgoing_buffer_size_size;

	// size of the outgoing buffer
	// plus enough space for digits holding the outgoing buffer size
	outgoing_buffer = malloc(sizeof(char) * total_buffer_size);

	// send message length to client
	// Citation: was having difficulties figuring out integer to string conversion
	// used below stack overflow thread as reference for prefixing packets with msg size
	// https://stackoverflow.com/questions/8257714/how-to-convert-an-int-to-string-in-c
	str_outgoing_buffer_size = malloc(sizeof(char) * outgoing_buffer_size_size);
	sprintf(str_outgoing_buffer_size, "%d", outgoing_buffer_size);
	strcpy(outgoing_buffer, str_outgoing_buffer_size);
	strcat(outgoing_buffer, " ");    // add delimiting space for buffer size

	// concatentate file contents into buffer
	strcat(outgoing_buffer, input_file);

	// send out buffer
	if(send_all(data_sockfd, outgoing_buffer, &total_buffer_size) == -1){
		perror("sendall");
		printf("Only %d bytes sent due to error!\n", total_buffer_size);
	}

	free(outgoing_buffer);
	free(str_outgoing_buffer_size);
}

// Description: builds buffer to send working directory to client
// Argument(s): socket fd for data connection, array holding filenames and
// number of files in directory
// Returns: nothing
void send_directory(int data_sockfd, char **directory_array,
	int directory_array_size){
	char *outgoing_buffer = NULL;
	int outgoing_buffer_size = 0;
	int outgoing_buffer_size_size = 0;
	int total_buffer_size = 0;    // holds size of buffer + space needed for buffer size prefix
	char *str_outgoing_buffer_size = NULL;
	int i;    // iterator for getting size and building buffer


	// get total size of filenames
	for(i = 0; i < directory_array_size; i++){
		// plus 1 to add in newline character
		outgoing_buffer_size += (strlen(directory_array[i]) + 1);
	}
	// add +1 for null terminator
	outgoing_buffer_size++;

	// get size needed for string holding buffer size
	outgoing_buffer_size_size = snprintf(NULL, 0, "%d", outgoing_buffer_size);
	outgoing_buffer_size_size++; // add space to delimit buffer size

	total_buffer_size = outgoing_buffer_size + outgoing_buffer_size_size;

	// size of the outgoing buffer
	// plus enough space for digits holding the outgoing buffer size
	outgoing_buffer = malloc(sizeof(char) * total_buffer_size);

	// send message length to client
	// Citation: was having difficulties figuring out integer to string conversion
	// used below stack thread as reference for prefixing packets with msg size
	// https://stackoverflow.com/questions/8257714/how-to-convert-an-int-to-string-in-c
	str_outgoing_buffer_size = malloc(sizeof(char) * outgoing_buffer_size_size);
	sprintf(str_outgoing_buffer_size, "%d", outgoing_buffer_size);
	strcpy(outgoing_buffer, str_outgoing_buffer_size);
	strcat(outgoing_buffer, " "); // add space to delimit buffer size

	// for loop to build buffer
	for(i = 0; i < directory_array_size; i++){
		strcat(outgoing_buffer, directory_array[i]);
		strcat(outgoing_buffer, "\n");
	}

	// send out buffer
	if(send_all(data_sockfd, outgoing_buffer, &total_buffer_size) == -1){
		perror("sendall");
		printf("Only %d bytes sent due to error!\n", total_buffer_size);
	}

	free(outgoing_buffer);
	free(str_outgoing_buffer_size);
}

int main(int argc, char *argv[])
{
	// silence warning for argc
	(void)argc;

	// variables for control connection setup
	int sockfd, control_fd;  // listen on sock_fd, new connection on new_fd
	char *serv_port;    // server port to listen on
	struct sockaddr_storage their_addr; // connector's address information
	socklen_t sin_size;
	char s[INET6_ADDRSTRLEN];

	// variables for data connecton setup
	int data_sockfd = NULL;
	int max_rec_buffer = 505;    // max size buffer that can be recieved
	char rec_buffer[max_rec_buffer];    // holds message coming from client
	char client_port[6];    // most digits a port num can have is 5
	char client_name[239];
	char req_filename[MAXFILENAME + 1];    // max filename size is 255
	char ft_option[3];    // options are only 2 chars long

	// variables for ft
	int directory_size = 0;
	size_t file_size;
	char **directory = NULL;
	char *input_file = NULL;
	char *file_error = "15 FILE NOT FOUND\0";    // error for file not existing
	char *option_error = "15 INVALID OPTION\0";    // error for invalid option
	FILE *input_fstream = NULL;
	int i;    // iterator for allocating mem.

	// get port num for server to run on from cmd line arg
	serv_port = argv[1];

	// setup control connection
	sockfd = control_setup(serv_port);


	printf("Server open on %s\n", serv_port);

	while(1) {  // main accept() loop
		sin_size = sizeof their_addr;
		control_fd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size);
		if (control_fd == -1) {
			perror("accept");
			continue;
		}

		inet_ntop(their_addr.ss_family,
			get_in_addr((struct sockaddr *)&their_addr),
			s, sizeof s);

		printf("Connection from %s\n", s);

		if (!fork()) { // this is the child process
			close(sockfd); // child doesn't need the listener

			// BUFFER //
			// get buffer from client
			if(recv(control_fd, rec_buffer, max_rec_buffer, 0) == -1)
				perror("receive");

			close(control_fd);    // close out control connection


			// parse buffer sent from client to get info
			parse_buffer(rec_buffer, client_name, ft_option,
				req_filename, client_port);

			// LIST OPTION //
			// if '-l' (list) option...
			if(ft_option[0] == '-' && ft_option[1] == 'l'){

				// get directory
				directory_size = get_directory_size();

				// allocate enough space for directory
				directory =  (char **) malloc(sizeof(char *) * directory_size);
				for(i = 0; i < directory_size; i++){
					directory[i] = (char *) malloc(MAXFILENAME);
				}

				get_directory(directory);

				// setup data connection
				data_sockfd = data_setup(client_name, client_port);

				printf("List directory requested on port %s.\n", client_port);
				printf("Sending directory contents to %s:%s.\n", client_name
					, client_port);

				// send dir back to client
				send_directory(data_sockfd, directory, directory_size);

				// free mem allocated for directory
				for(i = 0; i < directory_size; i++){
					free(directory[i]); 
				}
				free(directory);
				directory = NULL;
			}

			// GET OPTION //
			// if '-g' (get) option...
			else if(ft_option[0] == '-' && ft_option[1] == 'g'){

				// setup data connection
				data_sockfd = data_setup(client_name, client_port);

				input_fstream = fopen(req_filename, "r");

				if(input_fstream == NULL){
					// throw up internal error message
					printf("File not found.\n");
					printf("Sending error message to %s:%s.\n", client_name
					, client_port);

					// send error message to client
					if(send(data_sockfd, file_error, 18, 0))
						perror("send");

					exit(0);
				}

				// get file size in prep for sending
				file_size = get_file_size(req_filename);

				// allocate memory for outgoing buffer based off file size
				// +1 for null terminator
				input_file = malloc(sizeof(char) * file_size + 1);

				// check memory allocation
				if(input_file == NULL){
					printf("Error! Memory not allocated for holding input file contents.\n");
				}

				// get file contents from stream
				get_file(input_fstream, input_file, file_size);

				// file stream not needed anymore
				fclose(input_fstream);

				printf("File \"%s\" requested on port %s.\n", req_filename, client_port);
				printf("Sending \"%s\" to %s:%s.\n", req_filename, client_name
					, client_port);

				// send file contents
				send_file(data_sockfd, input_file, file_size);

				// file contents not needed anymore
				free(input_file);
				input_file = NULL;
			}

			// INVALID OPTION //
			// invalid option sent by client
			else{
					// throw up internal error message
					printf("Invalid option from client.\n");
					printf("Sending error message to %s:%s.\n", client_name
					, client_port);

					// send error message to client
					if(send(data_sockfd, option_error, 18, 0))
						perror("send");

					exit(0);
			}

			exit(0);
		}

		close(control_fd);  // parent doesn't need this
	}

	return 0;
}

