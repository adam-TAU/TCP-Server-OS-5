



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


#define bool int
#define true 1
#define false 0


/*************** GLOBAL VARIABLES ******************/
unsigned long pcc_total[95];
int finished = false;
int processing = false;
/***************************************************/





/*************** AUXILIARY FUNCTION DECLARATIONS ***************/
/* Print an error, and in case <terminate> is true, any errors will
 * terminate the calling process. */
static void print_err(char* error_message, bool terminate);

/* Prints the statistics of the printable characters' across all connecions,
 * and exit the program with exit status 0*/
static void print_stats(void);

/* Given an array of statistics holding the printable characters' info
 * for a specific connection named <pcc_tmp>, update the toatl array of
 * statistics holding the printable characters' info across all connecions */
static void update_total(unsigned long pcc_tmp[]);

/* returns the number of printable characters in <data_buff>, and increments each 
 * printable character's index equivalent in <pcc_tmp> */
static unsigned int update_tmp(char data_buff[], unsigned int size, unsigned long pcc_tmp[]);

/* This function is called upon receiving an EOF or a TCP error when powering
 * a syscall for receiving/sending data. Error messages are printed accordingly */
static void handle_connection_termination(bool is_ret_zero);

/* Signal handler for SIGINT */
static int server_sigint(void);
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

static void print_stats() {
	for (unsigned int i = 0; i < 95; i++) {
		printf("char '%c' : %lu times\n", (char)(i + 26), pcc_total[i]);
	}
	
	exit(0);
}

static void update_total(unsigned long pcc_tmp[]) {
	for (unsigned int i = 0; i < 95; i++) {
		pcc_total[i] += pcc_tmp[i];
	}
}

static unsigned int update_tmp(char data_buff[], unsigned int size, unsigned long pcc_tmp[]) {
	unsigned int printable_amount = 0;
	
	for (unsigned int i = 0; i < size; i++) {
		if (data_buff[i] >= 32 && data_buff[i] <= 126) {
			printable_amount++;
			pcc_tmp[(int)data_buff[i]]++;
		}
	}
	
	return printable_amount;
}

static void handle_connection_termination(bool is_ret_zero) {
	if (is_ret_zero) { // client unexpectedly killing
		print_err("Error: Received EOF, connection probably closed", false); // don't terminate
	} else { // other errors
		if (errno == ETIMEDOUT || errno == ECONNRESET || errno == EPIPE) {
			print_err("Error: Client connection terminated due to TCP errors", false); // don't terminate
		} else {
			print_err("Error: unexpected error when receiving/sending data", true); // do terminate
		}
	}
}

static int server_sigint(void) {
	if (processing) {
		finished = true;
	} else {
		print_stats();
	}
	
	return 0;
}



/*************** MAIN ******************/

// MINIMAL ERROR HANDLING FOR EASE OF READING

int main(int argc, char *argv[])
{
	int totalsent = -1;
	int nsent     = -1;
	int listenfd  = -1;
	int connfd    = -1;

	struct sockaddr_in serv_addr;
	struct sockaddr_in my_addr;
	struct sockaddr_in peer_addr;
	socklen_t addrsize = sizeof(struct sockaddr_in );

	// datastructure
	unsigned long pcc_tmp[95];

	// parse args
	if (argc != 2) {
		// handle error	
	}

	unsigned short port = atoi(argv[1]); // transfer to 16 bit

	// fetch connections
	char data_buff[1000000];

	listenfd = socket( AF_INET, SOCK_STREAM, 0 );
	memset( &serv_addr, 0, addrsize );

	serv_addr.sin_family = AF_INET;
	// INADDR_ANY = any local machine address
	serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	serv_addr.sin_port = htons(port);

	if( 0 != bind( listenfd,
				(struct sockaddr*) &serv_addr,
				addrsize ) )
	{
		printf("\n Error : Bind Failed. %s \n", strerror(errno));
		return 1;
	}

	if( 0 != listen( listenfd, 10 ) )
	{
		printf("\n Error : Listen Failed. %s \n", strerror(errno));
		return 1;
	}

	while( !finished )
	{
		
		
		// Accept a connection.
		// Can use NULL in 2nd and 3rd arguments
		// but we want to print the client socket details
		connfd = accept( listenfd,
				(struct sockaddr*) &peer_addr,
				&addrsize);
		processing = true;

		if( connfd < 0 )
		{
			printf("\n Error : Accept Failed. %s \n", strerror(errno));
			return 1;
		}

		getsockname(connfd, (struct sockaddr*) &my_addr,   &addrsize);
		getpeername(connfd, (struct sockaddr*) &peer_addr, &addrsize);
		printf( "Server: Client connected.\n"
				"\t\tClient IP: %s Client Port: %d\n"
				"\t\tServer IP: %s Server Port: %d\n",
				inet_ntoa( peer_addr.sin_addr ),
				ntohs(     peer_addr.sin_port ),
				inet_ntoa( my_addr.sin_addr   ),
				ntohs(     my_addr.sin_port   ) );

		// read the amount of characters that should be sent
		unsigned int bytes_to_read_n;
		char* bytes_to_read_buff = (char*) (&bytes_to_read_n);
		int notread = 0;
		int nread = 0;
		int totalread = 0;
		
		notread = sizeof(unsigned int);
		while ( notread > 0 ) {
			nread = read(connfd, bytes_to_read_buff + totalread, notread);
			// check if error occured (client closed connection?)
			if (nread <= 0) {
				handle_connection_termination(nread == 0); // handle the error accordingly
				continue; // ignore this connection (doesn't update pcc_total)
			}
			
			totalread += nread;
			notread -= nread;
		}

		unsigned int bytes_to_read_h = ntohl(bytes_to_read_n);
		
		// read the whole file
		unsigned int printable_amount = 0;
		int notread_from_file = bytes_to_read_h;
		nread = 0;
		totalread = 0;
		
		while ( notread_from_file > 0 ) {
			
			notread = (notread_from_file >= 1000000) ? 1000000 : notread_from_file;
			while ( notread > 0 ) {
				nread = read(connfd, data_buff + totalread, notread);
				// check if error occured (client closed connection?)
				assert(nread >= 0);
				
				totalread += nread;
				notread -= nread;
			}
			
			// process characters read from buffer
			printable_amount += update_tmp(data_buff, totalread, pcc_tmp);
		}


		// write time
		nsent = 0;
		totalsent = 0;
		int notwritten = sizeof(unsigned int);
		printable_amount = htonl(printable_amount);
		char* printable_amount_buff = (char*) (&printable_amount);

		// keep looping until nothing left to write
		while( notwritten > 0 )
		{
			// notwritten = how much we have left to write
			// totalsent  = how much we've written so far
			// nsent = how much we've written in last write() call */
			nsent = write(connfd,
					printable_amount_buff + totalsent,
					notwritten);
			// check if error occured (client closed connection?)
			assert( nsent >= 0);
			printf("Server: wrote %d bytes\n", nsent);

			totalsent  += nsent;
			notwritten -= nsent;
		}
		
		// update pcc_total
		update_total(pcc_tmp);

		// close socket
		close(connfd);
		processing = false;
	}
	
	print_stats(); // exits with 0 status
}




