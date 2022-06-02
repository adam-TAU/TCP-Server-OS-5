

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
static void recv_data(int sockfd, void *buff, unsigned long size);

/* This function receives an already connected socket with its file descriptor
 * of <sockfd>, the amount of bytes to send <size>, and the source of bytes
 * to send named <buff>, and sends all of the bytes to the server on the 
 * other end */
static void send_data(int sockfd, void *buff, unsigned long size);

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

static void recv_data(int sockfd, void *buff, unsigned long size) {
	int notread = 0;
	int nread = 0;
	int totalread = 0;
	
	notread = size;
	while ( notread > 0 ) {
		nread = read(sockfd, buff + totalread, notread);
		// check if error occured (client closed connection?)
		if (nread < 0) {
			print_err("Error: Couldn't read from socket", true);
		}
		
		totalread += nread;
		notread -= nread;
	}
}

static void send_data(int sockfd, void *buff, unsigned long size) {
	unsigned long nsent = 0;
	unsigned long totalsent = 0;
	unsigned long notwritten = size;
	
	while( notwritten > 0 ) {
		// notwritten = how much we have left to write
		// totalsent  = how much we've written so far
		// nsent = how much we've written in last write() call */
		nsent = write(sockfd,
				buff + totalsent,
				notwritten);
		// check if error occured (client closed connection?)
		if (nsent < 0) {
			print_err("Error: Couldn't send data through socket", true);
		}

		printf("Server: wrote %lu bytes\n", nsent);

		totalsent  += nsent;
		notwritten -= nsent;
	}
}

static void send_file(int sockfd, int file_fd) {

	struct stat sb;

	// send the amount of bytes we will send
	if ( -1 == fstat(file_fd, &sb) ) {
		print_err("Error: Couldn't `stat` the supplied file", true);
	}
	
	unsigned long number_bytes_send_h = sb.st_size;
	unsigned long number_bytes_send_n = htonl(number_bytes_send_h);
	void* number_bytes_send_n_buff = (void*) (&number_bytes_send_n);
	send_data(sockfd, number_bytes_send_n_buff, sizeof(unsigned long));
	
	
	char data_buff[1000000]; // 1MB buffer
	unsigned long bytes_read_from_file = 0;
	unsigned long file_notread = number_bytes_send_h;
	
	while( file_notread > 0 ) {
		
		bytes_read_from_file = read(file_fd, data_buff, 1000000);
		if ( bytes_read_from_file < 0 ) {
			// handle error
		}
	
		send_data(sockfd, data_buff, bytes_read_from_file);		
		file_notread -= bytes_read_from_file;
	}
}
/************************************************************/





/*************** MAIN ******************/
int main(int argc, char *argv[]) {
	
	struct sockaddr_in serv_addr; // where we Want to get to
	struct sockaddr_in my_addr;   // where we actually connected through 
	struct sockaddr_in peer_addr; // where we actually connected to
	socklen_t addrsize = sizeof(struct sockaddr_in );
	
	int  sockfd     = -1;

	if (argc != 4) {
		errno = EINVAL;
		print_err("Error: Not enough arguments passed", true);
	}

	// parse arguments
	char* ip_addr = argv[1];
	unsigned short port = atoi(argv[2]); // transfer to 16 bit
	char* file_path = argv[3]; // try opening the file and return an error accordingly 

	// open dedicated file
	int fd;
	if ( -1 == (fd = open(file_path, O_RDONLY)) ) {
		print_err("Error: Couldn't open the file given as a parameter", true);
		// handle error
	}

	// create tcp connection to server on port
	if( (sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		print_err("Error: Couldn't create a socket", true);
	}

	// print socket details
	getsockname(sockfd,
			(struct sockaddr*) &my_addr,
			&addrsize);
	printf("Client: socket created %s:%d\n",
			inet_ntoa((my_addr.sin_addr)),
			ntohs(my_addr.sin_port));

	memset(&serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(port); // Note: htons for endiannes
	serv_addr.sin_addr.s_addr = inet_addr(ip_addr); // hardcoded...

	printf("Client: connecting...\n");
	// Note: what about the client port number?
	// connect socket to the target address
	if( connect(sockfd,	(struct sockaddr*) &serv_addr, sizeof(serv_addr)) < 0)	{
		print_err("Error: Couldn't connect to server", true);
	}

	// print socket details again
	getsockname(sockfd, (struct sockaddr*) &my_addr,   &addrsize);
	getpeername(sockfd, (struct sockaddr*) &peer_addr, &addrsize);
	printf("Client: Connected. \n"
			"\t\tSource IP: %s Source Port: %d\n"
			"\t\tTarget IP: %s Target Port: %d\n",
			inet_ntoa((my_addr.sin_addr)),    ntohs(my_addr.sin_port),
			inet_ntoa((peer_addr.sin_addr)),  ntohs(peer_addr.sin_port));
	
	// send the bytes from the file
	send_file(sockfd, fd);

	// read the amount of printable characters that the server recognized in the file
	unsigned long printable_chars_n;
	recv_data(sockfd, &printable_chars_n, sizeof(unsigned long));
	unsigned long printable_chars_h = ntohl(printable_chars_n);
	
	// print the amount of printable characters in the supplied file
	printf("# of printable characters: %lu\n", printable_chars_h);
		
	// close socket and exit
	close(sockfd); // is socket really done here?
	exit(0);
}
