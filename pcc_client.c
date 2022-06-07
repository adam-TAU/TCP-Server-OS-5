

/* Submitter: Adam
 * Operating Systems 2022A
 * Tel Aviv University
 ==============================================
 * A simple TCP server 
 */
 
 
 
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>


#define bool int
#define true 1
#define false 0



/*************** AUXILIARY FUNCTION DECLARATIONS ***************/
/* Print an error, and in case <terminate> is true, any errors will
 * terminate the calling process. */
static void print_err(char* error_message, bool terminate);

/* This function receives an already connected socket with its file descriptor
 * of <sockfd>, the amount of bytes to receive named <size>, and the buffer
 * to save the sent bytes into named <buff> */
static void recv_data(int sockfd, void *buff, uint64_t size);

/* This function receives an already connected socket with its file descriptor
 * of <sockfd>, the amount of bytes to send <size>, and the source of bytes
 * to send named <buff>, and sends all of the bytes to the server on the 
 * other end */
static void send_data(int sockfd, void *buff, uint64_t size);

/* This function receives an already connected socket with its file descriptor
 * of <sockfd>, and a file descriptor named <file_fd> of the file we want to send
 * over the socket, and sends all of the contents of the file at <file_fd> over
 * the socket */
static void send_file(int sockfd, int file_fd);
/**************************************************/





/*************** AUXILIARY FUNCTION DEFINITIONS ***************/
static void print_err(char* error_message, bool terminate) {
	int tmp_errno = errno;
	perror(error_message); // this basically prints error_message, with <strerror(errno)> appended to it */
	errno = tmp_errno;
	
	if (terminate) {
		exit(1);
	}
}

static void recv_data(int sockfd, void *buff, uint64_t size) {
	int notread = size;
	int nread = 0;
	int totalread = 0;
	
	while ( notread > 0 ) {
	
		if ( 0 > (nread = read(sockfd, buff + totalread, notread)) ) { // if an error occured reading data
			print_err("Error: Couldn't read from socket", true);
		} else { // if no error occured when reading the data, advance
			totalread += nread;
			notread -= nread;
		}
		
	}
}

static void send_data(int sockfd, void *buff, uint64_t size) {
	uint64_t nsent = 0; // how much we've written in last write() call
	uint64_t totalsent = 0; // how much we've written so far
	uint64_t notwritten = size; // how much we have left to write
	
	while( notwritten > 0 ) {
	
		if ( 0 > (nsent = write(sockfd, buff + totalsent,	notwritten)) ) { // if an error occurred sending data
			print_err("Error: Couldn't send data through socket", true);
		} else { // if no error occured when sending the data, advance

			totalsent  += nsent;
			notwritten -= nsent;
		}
		
	}
}

static void send_file(int sockfd, int file_fd) {
	uint64_t number_bytes_send_h = -1; // the size of the file in bytes and host-endiann-ed

	// send the amount of bytes that the file that we send holds
	struct stat sb;
	if ( -1 == fstat(file_fd, &sb) ) { // if the stat of the file failed
		print_err("Error: Couldn't `stat` the supplied file", true);
	} else { // if the stat of the file succeeded, extract the file size, and send it over the connection
		number_bytes_send_h = sb.st_size; // host-endiann-ed size
		uint64_t number_bytes_send_n = htonl(number_bytes_send_h); // network-endiann-ed size
		send_data(sockfd, &number_bytes_send_n, sizeof(uint64_t)); // sending the size
	}
	
	// send the file in buffers to the server
	char data_buff[1000000]; // 1MB buffer
	uint64_t bytes_read_from_file = 0; // the amount of bytes we've currently read from the file
	uint64_t file_notread = number_bytes_send_h; // the amount of bytes in the file that we still need to send 
	
	while ( file_notread > 0 ) {
		
		bytes_read_from_file = read(file_fd, data_buff, 1000000); // read the next 1MB
		if ( bytes_read_from_file < 0 ) { // if the read failed
			print_err("Error: Couldn't read from file", true);
		} else { // if the read succeeded, send the 1MB buffer and advance
			send_data(sockfd, data_buff, bytes_read_from_file);		
			file_notread -= bytes_read_from_file;
		}
	}
}
/************************************************************/





/*************** MAIN ******************/
int main(int argc, char *argv[]) {
	
	
	int  sockfd = -1; // our connected socket fd
	int file_fd = -1; // our file fd
	struct sockaddr_in serv_addr; // where we Want to get to
	
	// validating the arguments amount
	if (argc != 4) {
		errno = EINVAL;
		print_err("Error: Not enough arguments passed", true);
	}

	// parse arguments
	char* ip_addr = argv[1];
	uint16_t port = atoi(argv[2]); // transfer to 16 bit
	char* file_path = argv[3]; // try opening the file and return an error accordingly 

	// open dedicated file
	if ( -1 == (file_fd = open(file_path, O_RDONLY)) ) {
		print_err("Error: Couldn't open the file given as a parameter", true);
	}

	// create tcp connection to server on port
	if( 0 > (sockfd = socket(AF_INET, SOCK_STREAM, 0)) ) {
		print_err("Error: Couldn't create a socket", true);
	}

	// configuring the address of our listening socket
	memset(&serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(port); // htons for endiannes
	serv_addr.sin_addr.s_addr = inet_addr(ip_addr); // hardcoded...
	
	// connect socket to the target address
	if( connect(sockfd,	(struct sockaddr*) &serv_addr, sizeof(serv_addr)) < 0)	{
		print_err("Error: Couldn't connect to server", true);
	}
	
	// send the bytes from the file
	send_file(sockfd, file_fd);

	// read the amount of printable characters that the server recognized in the file
	uint64_t printable_chars_n;
	recv_data(sockfd, &printable_chars_n, sizeof(uint64_t));
	uint64_t printable_chars_h = ntohl(printable_chars_n);
	
	// print the amount of printable characters in the supplied file
	printf("# of printable characters: %lu\n", printable_chars_h);
		
	// close socket and exit
	close(sockfd);
	exit(0);
}
