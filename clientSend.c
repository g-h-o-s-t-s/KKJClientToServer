#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <errno.h>
#include <sys/ioctl.h>
#define HOST "localhost"

char* readMessage(int fd){
    int status = -1;
    int retsize = 100;
    char* buffer = malloc(sizeof(char) * retsize);
    buffer[0] = '\0';
    int numread = 0;
	char c;
	int count;
    do{
        status = read(fd, &c, 1);
		if(status == -1) printf("READ ERROR\n");
        numread += status;
        while(numread > retsize){
            retsize *= 2;
            buffer = realloc(buffer, retsize);
        }
        if(status > 0) {
			buffer[numread - 1] = c;
			ioctl(fd, FIONREAD, &count);
        }
    } while(count > 0);
    buffer[numread] = '\0';
    return buffer;
}

void writer(int fd, char *buff, int len) {
	int cur = 0;
	while (cur < len) {
		int next = write(fd, buff + cur, len - cur);
		if (next == -1) {
			fprintf(stderr, "%s", strerror(errno));
			exit(EXIT_FAILURE);
		}
		cur += next;
	}
}

void communicate(int sock)
{
	//read m1
	char* m1 = readMessage(sock);
	printf("From Server... %s\n", m1);
	free(m1);
	//send m2
	char* m2 = "REG|12|Who's there?|";
	writer(sock, m2, strlen(m2));
	printf("Client Sent... %s\n", m2);
	//read m3
	char* m3 = readMessage(sock);
	printf("From Server... %s\n", m3);
	free(m3);
	//send m4
	char* m4 = "REG|14|Dijkstra, who?|";
	writer(sock, m4, strlen(m4));
	printf("Client Sent... %s\n", m4);
	//read m5
	char* m5 = readMessage(sock);
	printf("From Server... %s\n", m5);
	free(m5);
	//send m6
	char* m6 = "REG|4|Ahh!|";
	writer(sock, m6, strlen(m6));
	printf("Client Sent... %s\n", m6);
}

int main(int argc, char **argv)
{
	struct addrinfo hints, *address_list, *addr;
	int error;
	int sock;
	
	if (argc < 2) {
		printf("Usage: %s [port]\n", argv[0]);
		exit(EXIT_FAILURE);
	}
	// we need to provide some additional information to getaddrinfo using hints
	// we don't know how big hints is, so we use memset to zero out all the fields
	memset(&hints, 0, sizeof(hints));
	// indicate that we want any kind of address
	// in practice, this means we are fine with IPv4 and IPv6 addresses
	hints.ai_family = AF_UNSPEC;
	// we want a socket with read/write streams, rather than datagrams
	hints.ai_socktype = SOCK_STREAM;
	// get a list of all possible ways to connect to the host
	// argv[1] - the remote host
	// argv[2] - the service (by name, or a number given as a decimal string)
	// hints   - our additional requirements
	// address_list - the list of results
	error = getaddrinfo(HOST, argv[1], &hints, &address_list);
	if (error) {
		fprintf(stderr, "%s", gai_strerror(error));
		exit(EXIT_FAILURE);
	}
	// try each of the possible connection methods until we succeed
	for (addr = address_list; addr != NULL; addr = addr->ai_next) {
		// attempt to create the socket
		sock = socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol);
		// if we somehow failed, try the next method
		if (sock < 0) continue;
		// try to connect to the remote host using the socket
		if (connect(sock, addr->ai_addr, addr->ai_addrlen) == 0) {
			// we succeeded, so break out of the loop
			break;
		}
		// we weren't able to connect; close the socket and try the next method		
		close(sock);
	}
	// if we exited the loop without opening a socket and connecting, halt
	if (addr == NULL) {
		fprintf(stderr, "Could not connect to %s:%s\n", HOST, argv[1]);
		exit(EXIT_FAILURE);
	}
	// now that we have connected, we don't need the addressinfo list, so free it
	freeaddrinfo(address_list);

	//EXCHANGING MESSAGES
    communicate(sock);
    
	// close the socket
	close(sock);

	return EXIT_SUCCESS;	
}
