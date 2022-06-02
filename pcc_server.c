



/* Submitter: Adam
 * Operating Systems 2022A
 * Tel Aviv University
 ==============================================
 * A simple TCP server 
 */
 
 


#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>
#include <assert.h>
#include <signal.h>

#define bool int
#define true 1
#define false 0
#define CLIENT_TERMINATED -1
#define IS_PRINTABLE(x) ((x <= 126) && (x >= 32))

/*************** GLOBAL VARIABLES ******************/
char file_data_buff[1000000] = {0};
unsigned long pcc_total[95] = {0};
int finished = false;
int processing = false;
/***************************************************/





/*************** AUXILIARY FUNCTION DECLARATIONS ***************/
/* Print an error, and in case <terminate> is true, any errors will
 * terminate the calling process. */
static void print_err(char* error_message, bool terminate);

/* This function receives an already connected socket with its file descriptor
 * of <sockfd>, the amount of bytes to receive named <size>, and the buffer
 * to save the sent bytes into named <buff>
 *
 * Return the amount of bytes recevied on success (>=0), or `CLIENT_TERMINATED`
 * if client terminated.
 * Other errors may terminate the program as a whole */
static unsigned long recv_data(int sockfd, void *buff, unsigned long size);

/* This function receives an already connected socket with its file descriptor
 * of <sockfd>, the amount of bytes to send <size>, and the source of bytes
 * to send named <buff>, and sends all of the bytes to the server on the 
 * other end.
 *
 * Return the amount of bytes sent on success (>=0), or `CLIENT_TERMINATED`
 * if client terminated.
 * Other errors may terminate the program as a whole */
static unsigned long send_data(int sockfd, void *buff, unsigned long size);

/* This function accepts a socket of a connection to a client named <sockfd>,
 * the size of the file that the client is supposed to send named <file_size>,
 * and receives all of the contents of the file until done. Througout reading
 * the sent file, it also maintains statistics over all of the printable 
 * characters read in the file.
 *
 * Return the amount of printable characters read on success (>=0), or `CLIENT_TERMINATED`
 * if client terminated.
 * Other errors may terminate the program as a whole */
static unsigned long receive_file(int sockfd, unsigned long file_size, unsigned long pcc_current[]);

/* Prints the printable characters' statistics stored in <pcc_stats>.
 * <pcc_total> stands for the statistics across all connecions.
 * if <terminate> equates to <true>, it exits the program with exit status 0 */
static void print_stats(unsigned long pcc_stats[], bool terminate);

/* Given an array of statistics holding the printable characters' info
 * for a specific connection named <pcc_current>, update the toatl array of
 * statistics holding the printable characters' info across all connecions */
static void update_pcc_total(unsigned long pcc_current[]);

/* returns the number of printable characters in <file_data_buff>, and increments each 
 * printable character's index equivalent in <pcc_current> */
static unsigned long update_pcc_current(char file_data_buff[], unsigned long size, unsigned long pcc_current[]);

/* This function is called upon receiving an EOF or a TCP error when powering
 * a syscall for receiving/sending data. Error messages are printed accordingly */
static void handle_connection_termination(bool is_ret_zero);

/* Signal handler for SIGINT */
static void server_sigint(int);
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

static unsigned long recv_data(int sockfd, void *buff, unsigned long size) {
	int notread = 0;
	int nread = 0;
	int totalread = 0;
	
	notread = size;
	while ( notread > 0 ) {
		nread = read(sockfd, buff + totalread, notread);
		// check if error occured (client closed connection?)
		if (nread <= 0) {
			if (errno != EINTR && nread != 0) { // if the error is EINTR, simply ignore it (SIG_INT handler)
				handle_connection_termination(nread == 0); // handle the error accordingly
				goto client_error;
			}
		}
		
		totalread += nread;
		notread -= nread;
	}
	
	return totalread;
	
client_error:
	return CLIENT_TERMINATED;
}

static unsigned long send_data(int sockfd, void *buff, unsigned long size) {
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
		if (nsent <= 0) {
			if (errno != EINTR && nsent != 0) { // if the error is EINTR, simply ignore it (SIG_INT handler)
				handle_connection_termination(false);
				goto client_error;
			}
		}

		// OPTIONAL: TODO: REMOVE
		printf("Server: wrote %lu bytes\n", nsent);

		totalsent  += nsent;
		notwritten -= nsent;
	}
	
	return totalsent;
	
client_error:
	return CLIENT_TERMINATED;
}

static unsigned long receive_file(int sockfd, unsigned long file_size, unsigned long pcc_current[]) {
	unsigned long printable_chars = 0;
	unsigned long notread_from_file = file_size;

	while ( notread_from_file > 0 ) {

		unsigned long notread = (notread_from_file >= 1000000) ? 1000000 : notread_from_file;
		if ( CLIENT_TERMINATED == recv_data(sockfd, file_data_buff, notread) ) {
			goto client_error;
		}
		notread_from_file -= notread;
		
		// process characters read from buffer
		printable_chars += update_pcc_current(file_data_buff, notread, pcc_current);
	}
	
	return printable_chars;
	
client_error:
	return CLIENT_TERMINATED;
}

static void print_stats(unsigned long pcc_stats[], bool terminate) {
	for (unsigned int i = 0; i < 95; i++) {
		printf("char '%c' : %lu times\n", (char)(i + 32), pcc_stats[i]);
	}
	
	if (terminate) {
		exit(0);
	}
}

static void update_pcc_total(unsigned long pcc_current[]) {
	for (unsigned int i = 0; i < 95; i++) {
		pcc_total[i] += pcc_current[i];
	}
}

static unsigned long update_pcc_current(char file_data_buff[], unsigned long size, unsigned long pcc_current[]) {
	unsigned long printable_chars = 0;

	for (unsigned long i = 0; i < size; i++) {
		if (IS_PRINTABLE(file_data_buff[i])) {
			printable_chars++;
			pcc_current[(int)file_data_buff[i] - 32]++;
		}
	}
	
	return printable_chars;
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

static void server_sigint(int sig) {
	if (processing) {
		finished = true;
	} else {
		print_stats(pcc_total, true);
	}
}



/*************** MAIN ******************/

// MINIMAL ERROR HANDLING FOR EASE OF READING

int main(int argc, char *argv[]) {

	int listenfd  = -1;
	int connfd    = -1;
	
	struct sockaddr_in serv_addr;
	struct sockaddr_in my_addr;
	struct sockaddr_in peer_addr;
	socklen_t addrsize = sizeof(struct sockaddr_in );
	
	// connecting signal handler
	struct sigaction sa_int;
	sa_int.sa_handler = server_sigint; // make the handling of SIGINT default again (I.E. terminate upon a SIGINT)
	if ( 0 > sigaction(SIGINT, &sa_int, 0) ) print_err("Error: Couldn't set SIG_INT handler", true);
	
	// parse args
	if (argc != 2) {
		errno = EINVAL;
		print_err("Error: Not enough arguments passed", true);
	}

	unsigned short port = atoi(argv[1]); // transfer to 16 bit

	// create listening socket
	if ( -1 == (listenfd = socket( AF_INET, SOCK_STREAM, 0 )) ) {
		print_err("Error: Couldn't open a socket", true);
	}
	
	memset( &serv_addr, 0, addrsize );

	serv_addr.sin_family = AF_INET;
	// INADDR_ANY = any local machine address
	serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	serv_addr.sin_port = htons(port);

	// enabling port reuse after server terminates
	int option_value = 1;
	if ( -1 == (setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &option_value, sizeof(int))) ) {
		print_err("Error: Couldn't set socket options", true);
	}

	if( 0 != bind( listenfd,
				(struct sockaddr*) &serv_addr,
				addrsize ) ) {
		printf("\n Error : Bind Failed. %s \n", strerror(errno));
		return 1;
	}

	if( 0 != listen( listenfd, 10 ) ) {
		printf("\n Error : Listen Failed. %s \n", strerror(errno));
		return 1;
	}

	unsigned long pcc_current[95];
	
	while( !finished ) {
		
		memset(pcc_current, 0, 95 * sizeof(unsigned long));

		// Accept a connection.
		// Can use NULL in 2nd and 3rd arguments
		// but we want to print the client socket details
		if ( -1 == (connfd = accept(listenfd, (struct sockaddr*) &peer_addr, &addrsize)) ) {
			if (errno == EINTR) { // caused by our signal handler (every handler on linux has SA_RESTART turned on by default)
				break; // exit loop
			} else { // if it's a non-sigint-related error
				print_err("Error: Couldn't accept a connection on the port we are listening", true);
			}	
		}
		
		// OPTIONAL: TODO: REMOVE
		getsockname(connfd, (struct sockaddr*) &my_addr,   &addrsize);
		getpeername(connfd, (struct sockaddr*) &peer_addr, &addrsize);
		printf( "Server: Client connected.\n"
				"\t\tClient IP: %s Client Port: %d\n"
				"\t\tServer IP: %s Server Port: %d\n",
				inet_ntoa( peer_addr.sin_addr ),
				ntohs(     peer_addr.sin_port ),
				inet_ntoa( my_addr.sin_addr   ),
				ntohs(     my_addr.sin_port   ) );
		
		// signal to other function calls that we started processing
		processing = true;

		// read the amount of characters that the file being sent will hold
		unsigned long file_size_n;
		if ( CLIENT_TERMINATED == recv_data(connfd, &file_size_n, sizeof(unsigned long)) ) {
			processing = false;
			continue;
		}
		unsigned long file_size_h = ntohl(file_size_n);
		
		// read the file sent and fetch the amount of printable characters in that file
		unsigned long printable_chars_h = 0;
		if ( CLIENT_TERMINATED == (printable_chars_h = receive_file(connfd, file_size_h, pcc_current)) ) {
			processing = false;
			continue;
		}
		
		// send the amount of printable characters in the file sent to the client
		unsigned long printable_chars_n = htonl(printable_chars_h);
		if ( CLIENT_TERMINATED == send_data(connfd, &printable_chars_n, sizeof(unsigned long)) ) {
			processing = false;
			continue;
		}

		// close socket
		if (-1 == close(connfd)) {
			if (errno != EINTR) print_err("Error: Couldn't close a socket of a connection", true); 
		}
		
		// update pcc_total
		update_pcc_total(pcc_current);
		
		// turn off the flag of processing
		processing = false;
	}
	
	print_stats(pcc_total, true); // exits with 0 status
}
