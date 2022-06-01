

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

/* This function is called upon receiving an EOF or a TCP error when powering
 * a syscall for receiving/sending data. Error messages are printed accordingly */
static void handle_connection_termination(bool is_ret_zero);
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



static void handle_connection_termination(bool is_ret_zero) {
	if (is_ret_zero) { // client unexpectedly killing
		print_err("Error: Received EOF - connection may have been closed", false); // don't terminate
	} else { // other errors
		if (errno == ETIMEDOUT || errno == ECONNRESET || errno == EPIPE) {
			print_err("Error: Client connection terminated due to TCP errors", false); // don't terminate
		} else {
			print_err("Error: unexpected error when receiving/sending data", true); // do terminate
		}
	}
}




/*************** MAIN ******************/

// MINIMAL ERROR HANDLING FOR EASE OF READING

int main(int argc, char *argv[])
{
	struct stat sb;
	struct sockaddr_in serv_addr; // where we Want to get to
	struct sockaddr_in my_addr;   // where we actually connected through 
	struct sockaddr_in peer_addr; // where we actually connected to
	socklen_t addrsize = sizeof(struct sockaddr_in );
	
	
	int totalsent = -1;
	int nsent     = -1;
	int  sockfd     = -1;
	int notwritten = -1;
	int  bytes_read =  0;

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
	if( connect(sockfd,
				(struct sockaddr*) &serv_addr,
				sizeof(serv_addr)) < 0)	{
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

	// send the amount of bytes we will send
	if ( -1 == stat(file_path, &sb) ) {
		// handle error
	}
	
	unsigned long number_bytes_send_h = sb.st_size;
	unsigned long number_bytes_send_n = htonl(number_bytes_send_h);
	void* number_bytes_send_n_buff = (void*) (&number_bytes_send_n);
	
	nsent = 0;
	totalsent = 0;
	notwritten = sizeof(unsigned long);
	// keep looping until nothing left to write
	while( notwritten > 0 )
	{
		// notwritten = how much we have left to write
		// totalsent  = how much we've written so far
		// nsent = how much we've written in last write() call */
		nsent = write(sockfd,
				number_bytes_send_n_buff + totalsent,
				notwritten);
		// check if error occured (client closed connection?)
		if (nsent <= 0) {
			handle_connection_termination(false);
		}
		
		printf("Server: wrote %d bytes\n", nsent);

		totalsent  += nsent;
		notwritten -= nsent;
	}
	
	// send the bytes from the file
	char data_buff[1000000]; // 1MB buffer
	unsigned long bytes_read_from_file = 0;
	unsigned long total_file_sent = 0;
	unsigned long file_notread = number_bytes_send_h;
	while( file_notread > 0 ) {
		
		bytes_read_from_file = read(fd, data_buff, 1000000);
		if ( bytes_read_from_file < 0 ) {
			// handle error
		}
	
		nsent = 0;
		totalsent = 0;
		notwritten = bytes_read_from_file;
		
		while( notwritten > 0 )
		{
			// notwritten = how much we have left to write
			// totalsent  = how much we've written so far
			// nsent = how much we've written in last write() call */
			nsent = write(sockfd,
					data_buff + totalsent,
					notwritten);
			// check if error occured (client closed connection?)
			if (nsent <= 0) {
				handle_connection_termination(false);
			}

			printf("Server: wrote %d bytes\n", nsent);

			totalsent  += nsent;
			notwritten -= nsent;
		}
		
		file_notread -= bytes_read_from_file;
		total_file_sent += 0;
	}

	// read the amount of printable characters that the server recognized
	unsigned long printable_amount_n;
	void* printable_amount_n_buff = (void*) (&printable_amount_n);
	int notread = 0;
	int nread = 0;
	int totalread = 0;
	
	notread = sizeof(unsigned long);
	while ( notread > 0 ) {
		nread = read(sockfd, printable_amount_n_buff + totalread, notread);
		// check if error occured (client closed connection?)
		if (nread <= 0) {
			handle_connection_termination(nread == 0); // handle the error accordingly
			continue; // ignore this connection (doesn't update pcc_total)
		}
		
		totalread += nread;
		notread -= nread;
	}
	
	unsigned long printable_amount_h = ntohl(printable_amount_n);
	printf("# of printable characters: %lu\n", printable_amount_h);
		

	close(sockfd); // is socket really done here?
	exit(0);
}
