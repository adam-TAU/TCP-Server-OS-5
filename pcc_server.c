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
#include <endian.h>

#define bool int
#define true 1
#define false 0
#define CLIENT_TERMINATED -1
#define CHARS_RANGE 95
#define IS_PRINTABLE(x) ((x <= 126) && (x >= 32))

/*************** GLOBAL VARIABLES ******************/
char file_data_buff[1000000] = {0};
uint64_t pcc_total[CHARS_RANGE] = {0};
int finished = false;
socklen_t addrsize = sizeof(struct sockaddr_in); // the size for sockaddr_in
int connfd = -1; // stores the fd for the connected socket
/***************************************************/




/*************************************************************************
************************* CONTROL PERIPHERALS ****************************
**************************************************************************/
/* Print an error, and in case <terminate> is true, any errors will
 * terminate the calling process. */
static void print_err(char* error_message, bool terminate);

/* This function is called upon receiving an EOF or a TCP error when powering
 * a syscall for receiving/sending data. Error messages are printed accordingly */
static void handle_connection_termination(bool is_ret_zero);

/* close connection fd safely */
static void close_safe(int sockfd);

/* Sets the signal handler of SIGINT to be our `server_sigint` function */
static void set_sigint_handler(void);

/* Signal handler for SIGINT */
static void server_sigint(int);


/***************************************************************************
************************* COMMUNICATION HELPERS ****************************
****************************************************************************/
/* This function receives an already connected socket with its file descriptor
 * of <sockfd>, the amount of bytes to receive named <size>, and the buffer
 * to save the sent bytes into named <buff>
 *
 * Return the amount of bytes recevied on success (>=0), or `CLIENT_TERMINATED`
 * if client terminated.
 * Other errors may terminate the program as a whole */
static uint64_t recv_data(int sockfd, void *buff, uint64_t size);

/* This function receives an already connected socket with its file descriptor
 * of <sockfd>, the amount of bytes to send <size>, and the source of bytes
 * to send named <buff>, and sends all of the bytes to the server on the 
 * other end.
 *
 * Return the amount of bytes sent on success (>=0), or `CLIENT_TERMINATED`
 * if client terminated.
 * Other errors may terminate the program as a whole */
static uint64_t send_data(int sockfd, void *buff, uint64_t size);


/**************************************************************************
************************* AUXILIARY FUNCTIONS *****************************
***************************************************************************/
/* This function accepts a socket of a connection to a client named <sockfd>,
 * the size of the file that the client is supposed to send named <file_size>,
 * and receives all of the contents of the file until done. Througout reading
 * the sent file, it also maintains statistics over all of the printable 
 * characters read in the file.
 *
 * Return the amount of printable characters read on success (>=0), or `CLIENT_TERMINATED`
 * if client terminated.
 * Other errors may terminate the program as a whole */
static uint64_t receive_and_process_file(int sockfd, uint64_t file_size, uint64_t pcc_current[]);

/* Prints the printable characters' statistics stored in <pcc_stats>.
 * <pcc_total> stands for the statistics across all connecions.
 * if <terminate> equates to <true>, it exits the program with exit status 0 */
static void print_stats(uint64_t pcc_stats[], bool terminate);

/* Given an array of statistics holding the printable characters' info
 * for a specific connection named <pcc_current>, update the toatl array of
 * statistics holding the printable characters' info across all connecions */
static void update_pcc_total(uint64_t pcc_current[]);

/* returns the number of printable characters in <file_data_buff>, and increments each 
 * printable character's index equivalent in <pcc_current> */
static uint64_t update_pcc_current(char file_data_buff[], uint64_t size, uint64_t pcc_current[]);


/*********************************************************************
************************* MAIN MECHANISM *****************************
**********************************************************************/
/* processes files sent over connected socket induced from the listening socket <listenfd> */
static void process_connections(int listenfd);
/**************************************************/






/*************************************************************************
************************* CONTROL PERIPHERALS ****************************
**************************************************************************/
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
		print_err("Error: Received EOF - client may have been killed - unexpected connection close", false); // don't terminate
	} else { // other errors
		if (errno == ETIMEDOUT || errno == ECONNRESET || errno == EPIPE) {
			print_err("Error: Client connection terminated due to TCP errors", false); // don't terminate
		} else {
			print_err("Error: unexpected error when receiving/sending data", true); // do terminate
		}
	}
}

static void close_safe(int sockfd) {
	if (-1 == close(sockfd)) {
		if (errno != EINTR) print_err("Error: Couldn't close a socket of a connection", true); 
	}
}

static void set_sigint_handler(void) {
	struct sigaction sa_int;
	sa_int.sa_handler = server_sigint; // make the handling of SIGINT default again (I.E. terminate upon a SIGINT)
	if ( 0 > sigaction(SIGINT, &sa_int, 0) ) {
		print_err("Error: Couldn't set SIG_INT handler", true);
	}
}

static void server_sigint(int sig) {

	if (connfd == -1) { // if no client is currently being processed, simply terminate the program
		print_stats(pcc_total, true);
	} else { // if there is a client being processed, simply signal to the server to not accept any new connections
		finished = true; 
	}
}
/*************************************************************************/







/***************************************************************************
************************* COMMUNICATION HELPERS ****************************
****************************************************************************/
static uint64_t recv_data(int sockfd, void *buff, uint64_t size) {
	int notread = size; // how much we have left to read
	int nread = 0; // how much we've read so far
	int totalread = 0; // how much we've written in last read() call
	
	while ( notread > 0 ) {

		if ( 0 >= (nread = read(sockfd, buff + totalread, notread)) ) {
			if (errno == EINTR) { // if the error is EINTR, simply ignore it (SIG_INT handler) and redo the reading
				continue;
			} else { 
				handle_connection_termination(nread == 0); // terminate if not a TCP error or unexpected connection termination
				goto client_error; // this part is reached only if the error was a TCP error or an unexpected connection termination
			}
		} else { // if the reading succeeded, advance
			totalread += nread;
			notread -= nread;
		}
		
	}
	
	return totalread;
	
client_error:
	return CLIENT_TERMINATED;
}

static uint64_t send_data(int sockfd, void *buff, uint64_t size) {
	uint64_t nsent = 0; // how much we've written in last write() call
	uint64_t totalsent = 0; // how much we've written so far
	uint64_t notwritten = size; // how much we have left to write
	
	// start sending until no data left to send
	while( notwritten > 0 ) {
	
		if ( 0 >= (nsent = write(sockfd, buff + totalsent,	notwritten)) ) {
			if (errno == EINTR) { // if the error is EINTR, simply ignore it (SIG_INT handler) and redo the sending
				continue;
			} else { 
				handle_connection_termination(nsent == 0); // terminate if not a TCP error or unexpected connection termination
				goto client_error; // this part is reached only if the error was a TCP error or an unexpected connection termination
			}
		} else { // if writing succeeded, advance
		
			totalsent  += nsent;
			notwritten -= nsent;
		}
		
	}
	
	// return the amount of bytes sent
	return totalsent;
	
client_error:
	return CLIENT_TERMINATED;
}
/****************************************************************************/







/**************************************************************************
************************* AUXILIARY FUNCTIONS *****************************
***************************************************************************/
static uint64_t receive_and_process_file(int sockfd, uint64_t file_size, uint64_t pcc_current[]) {
	uint64_t printable_chars = 0; // how many printable characters were found in the file
	uint64_t notread_from_file = file_size; // the file size we expect to process

	while ( notread_from_file > 0 ) {
		
		// reading the next 1MB from the file
		uint64_t notread = (notread_from_file >= 1000000) ? 1000000 : notread_from_file;
		if ( CLIENT_TERMINATED == recv_data(sockfd, file_data_buff, notread) ) { // if the connection terminated unexpectedly 
			goto client_error;
		} else { // if the data was received without any errors, advance
			notread_from_file -= notread;
		}
		
		// process characters read from file into the buffer <file_data_buff>
		printable_chars += update_pcc_current(file_data_buff, notread, pcc_current);
	}
	
	// return the amount of printable characters processed
	return printable_chars; 
	
client_error:
	return CLIENT_TERMINATED;
}

static void print_stats(uint64_t pcc_stats[], bool terminate) {
	// printing the stats
	for (unsigned int i = 0; i < CHARS_RANGE; i++) {
		printf("char '%c' : %lu times\n", (char)(i + 32), pcc_stats[i]);
	}
	
	// terminating
	if (terminate) {
		exit(0);
	}
}

static void update_pcc_total(uint64_t pcc_current[]) { 
	// adding the the current statistics with the total statistics
	for (unsigned int i = 0; i < CHARS_RANGE; i++) {
		pcc_total[i] += pcc_current[i];
	}
}

static uint64_t update_pcc_current(char file_data_buff[], uint64_t size, uint64_t pcc_current[]) {
	uint64_t printable_chars = 0; // the amount of printable characters in <file_data_buff>

	// inserting the statistics of the printable characters in the given buffer into <pcc_current>
	for (uint64_t i = 0; i < size; i++) { 
		if (IS_PRINTABLE(file_data_buff[i])) { // if a printable character was found
			printable_chars++;
			pcc_current[(int)file_data_buff[i] - 32]++;
		}
	}
	
	// returning the amount of printable characters in the given buffer
	return printable_chars; 
}
/**************************************************************************/








/*********************************************************************
************************* MAIN MECHANISM *****************************
**********************************************************************/
static void process_connections(int listenfd) {
	uint64_t pcc_current[CHARS_RANGE]; // will hold the statistics for the current connection that is being processed
	
	
	while( !finished ) { // accepting connections until SIGINT arrives or an unexpected error terminated the program
		
		// zero-ing out the recent connection-based statistics
		memset(pcc_current, 0, CHARS_RANGE * sizeof(uint64_t));
		
		// Accept a connection.
		// Can use NULL in 2nd and 3rd arguments
		// but we want to print the client socket details
		if ( -1 == (connfd = accept(listenfd, NULL, NULL)) ) {
			if (errno == EINTR) { // must be caused by our signal handler (every handler on linux has SA_RESTART turned on by default)
				break; // exit loop
			} else { // if it's a non-sigint-related error
				print_err("Error: Couldn't accept a connection on the port we are listening", true);
			}	
		}

		// read the amount of characters that the file being sent will hold
		uint64_t file_size_n;
		if ( CLIENT_TERMINATED == recv_data(connfd, &file_size_n, sizeof(uint64_t)) ) {
			close_safe(connfd);
			continue;
		}
		uint64_t file_size_h = be64toh(file_size_n);
		
		// read the file sent and fetch the amount of printable characters in that file
		uint64_t printable_chars_h = 0;
		if ( CLIENT_TERMINATED == (printable_chars_h = receive_and_process_file(connfd, file_size_h, pcc_current)) ) {
			close_safe(connfd);
			continue;
		}
		
		// send the amount of printable characters in the file sent to the client
		uint64_t printable_chars_n = htobe64(printable_chars_h);
		if ( CLIENT_TERMINATED == send_data(connfd, &printable_chars_n, sizeof(uint64_t)) ) {
			close_safe(connfd);
			continue;
		}

		// close socket + update pcc_total
		update_pcc_total(pcc_current);
		close_safe(connfd);
		
		// signal to the handler that no client is currently being processed
		connfd = -1;
	}
}
/**********************************************************************/









/*************** MAIN ******************/
int main(int argc, char *argv[]) {

	int listenfd  = -1; // stores the fd for the listening socket
	struct sockaddr_in serv_addr; // server addreess
	
	// connecting signal handler
	set_sigint_handler();
	
	// parse args
	if (argc != 2) {
		errno = EINVAL;
		print_err("Error: Not enough arguments passed", true);
	}

	uint16_t port = atoi(argv[1]); // transfer to 16 bit

	// create listening socket
	if ( -1 == (listenfd = socket( AF_INET, SOCK_STREAM, 0 )) ) {
		print_err("Error: Couldn't open a socket", true);
	}

	// enabling port reuse after server terminates (Time Wait issue)
	int option_value = 1;
	if ( 0 != (setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &option_value, sizeof(int))) ) {
		print_err("Error: Couldn't set socket options", true);
	}

	// configuring our server address correctly and binding our listening socket to the server address
	memset( &serv_addr, 0, addrsize);
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	serv_addr.sin_port = htons(port);

	if( 0 != bind( listenfd, (struct sockaddr*) &serv_addr,	addrsize ) ) {
		print_err("Error: Couldn't bind a listening socket", true);
		return 1;
	}

	// listening on the socket for connections
	if( 0 != listen( listenfd, 10 ) ) {
		print_err("Error: Couldn't `listen` to socket", true);
		return 1;
	}

	// processing new connections
	process_connections(listenfd);
	
	// closing listening socket and printing statistics
	close_safe(listenfd);
	print_stats(pcc_total, true); // exits with 0 status
}
