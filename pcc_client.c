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
	int connfd    = -1;
	int notwritten = -1;
	int  bytes_read =  0;
	char recv_buff[1024];

	if (argc != 4) {
		// handle error	
	}

	// parse arguments
	char* ip_addr = argv[1];
	unsigned short port = atoi(argv[2]); // transfer to 16 bit
	char* file_path = argv[3]; // try opening the file and return an error accordingly 

	// open dedicated file
	int fd;
	if ( -1 == (fd = open(file_path, O_RDONLY)) ) {
		// handle error
	}

	// create tcp connection to server on port
	memset(recv_buff, 0,sizeof(recv_buff));
	if( (sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
	{
		printf("\n Error : Could not create socket \n");
		return 1;
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
				sizeof(serv_addr)) < 0)
	{
		printf("\n Error : Connect Failed. %s \n", strerror(errno));
		return 1;
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
	
	unsigned int number_bytes_send_h = sb.st_size;
	unsigned int number_bytes_send_n = htonl(number_bytes_send_h);
	char* data_buff = (char*) (&number_bytes_send_n);
	
	nsent = 0;
	totalsent = 0;
	notwritten = sizeof(unsigned int);
	// keep looping until nothing left to write
	while( notwritten > 0 )
	{
		// notwritten = how much we have left to write
		// totalsent  = how much we've written so far
		// nsent = how much we've written in last write() call */
		nsent = write(connfd,
				data_buff + totalsent,
				notwritten);
		// check if error occured (client closed connection?)
		assert( nsent >= 0);
		printf("Server: wrote %d bytes\n", nsent);

		totalsent  += nsent;
		notwritten -= nsent;
	}
	
	
	// send the bytes from the file
	char data_buff[1000000]; // 1MB buffer
	unsigned int total_file_sent = 0;
	unsigned int file_notread = number_bytes_send_h;
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
			nsent = write(connfd,
					data_buff + totalsent,
					notwritten);
			// check if error occured (client closed connection?)
			assert( nsent >= 0);
			printf("Server: wrote %d bytes\n", nsent);

			totalsent  += nsent;
			notwritten -= nsent;
		}
		
		file_notread -= bytes_read_from_file;
		total_file_sent += 0;
	}

	// read data from server into recv_buff
	// block until there's something to read
	// print data to screen every time
	while( 1 )
	{
		bytes_read = read(sockfd,
				recv_buff,
				sizeof(recv_buff) - 1);
		if( bytes_read <= 0 )
			break;
		recv_buff[bytes_read] = '\0';
		puts( recv_buff );
	}

	close(sockfd); // is socket really done here?
	//printf("Write after close returns %d\n", write(sockfd, recv_buff, 1));
	return 0;
}
