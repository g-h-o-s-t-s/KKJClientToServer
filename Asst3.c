/*-------------------------**
	Sagnik Mukherjee
	December 10, 2020
**-------------------------*/
#include <stdlib.h>
#include <stdio.h>

#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <errno.h>

#include <netdb.h>
#include <sys/socket.h>
#include <sys/ioctl.h>

#define BACKLOG 5
#define MAXSIZE 256
/*-----------------------------------*/
typedef struct sockaddr sockaddr;
typedef struct sockaddr_storage sockaddr_storage;
typedef struct addrinfo addrinfo;

typedef struct connection 
{
	int fd;
	sockaddr_storage addr;
	socklen_t addr_len;
} connection;
/*-----------------------------------*/
// Server-client communication functions "borrowed"
// from Prof. Menendez's example files from lectures,
// server.c and echos.c.
void acceptConnection(connection* conn, int sfd);
int server(char* port);
char* readMessage(int fd);
void writer(int fd, char* buff, int len);

int checkResponse1(char* str1);
int checkResponse2(char* str3, char* setup);
int checkResponse3(char* str5);

void printErrorMsg(char* errMsg);
void exitSocketOnErr(char* errMsg);
void sendErr(int errCode, int msgNum, int fd);

int processMsg(char* message);
void echo(connection* arg);

connection* currentConn;
/*-----------------------------------*/
int main(int argc, char** argv)
{
	if (argc != 2) 
	{
		printf("Client Syntax: %s [port]\n", argv[0]);
		exit(EXIT_FAILURE);
	}

	if (atoi(argv[1]) < 5000 || atoi(argv[1]) > 65536)
	{
		printf("Please connect to a port from 5000 - 65536.\n");
		exit(EXIT_FAILURE);
	}

	(void) server(argv[1]);
	return EXIT_SUCCESS;
}
/*-----------------------------------*/
void acceptConnection(connection* conn, int sfd)
{
	printf("Waiting for connection...\n");
	for (;;) 
	{
		// Initialize connection for child thread.
		conn = malloc(sizeof(connection));

		// addr_len is r/w parameter, holds actual length after accept call.
		// sfd is listening socket, addr of remote host, addr_len of address,
		// accept listens for any attempted connections.
		conn->addr_len = sizeof(sockaddr_storage);
		conn->fd = accept(sfd, (sockaddr*) &conn->addr, &conn->addr_len);

		if (conn->fd == -1) 
		{
			perror("accept()");
			continue;
		}

		currentConn = conn;
		echo(conn);
	}
}
/*-----------------------------------*/
int server(char* port)
{
	addrinfo hint, *address_list, *addr;
	connection* conn;
	int error, sfd;

	// Set initial hints.
	memset(&hint, 0, sizeof(addrinfo));
	hint.ai_family = AF_UNSPEC;
	hint.ai_socktype = SOCK_STREAM;
	hint.ai_flags = AI_PASSIVE;

	// Create locally hosted listening socket.
	error = getaddrinfo(NULL, port, &hint, &address_list);
	if (error != 0) 
	{
		fprintf(stderr, "getaddrinfo(): %s\n", gai_strerror(error));
		return -1;
	}

	for (addr = address_list; addr; addr = addr->ai_next) 
	{
		sfd = socket(addr->ai_family, addr->ai_socktype,
			addr->ai_protocol);

		// Retry with next method of socket creation if failure.
		if (sfd == -1) 
			continue;

		// Bind socket to port on localhost,
		// and set up queue for connections with listen(), use accept().
		if ((bind(sfd, addr->ai_addr, addr->ai_addrlen) == 0) &&
			(listen(sfd, BACKLOG) == 0))
			break;

		close(sfd);
	}

	// Unable to bind socket.
	if (!addr) 
	{
		fprintf(stderr, "Binding unsuccessful.\n");
		return -1;
	}

	// sfd value bound, listening for connection.
	freeaddrinfo(address_list);
	acceptConnection(conn, sfd);
	return 0;
}
/*-----------------------------------*/
char* readMessage(int fd)
{
	int charCount = 0;
	char* buff = malloc(sizeof(char) * MAXSIZE);

	// Message starts with either REG or ERR.
	for (charCount = 0; charCount < 4; ++charCount)
		read(fd, &buff[charCount], 1);

	if (buff[0] == 'R' 
		&& buff[1] == 'E' 
		&& buff[2] == 'G' 
		&& buff[3]=='|')
	{
		do {
			read(fd, &buff[charCount], 1);
			charCount++;
		} while (isdigit(buff[charCount - 1]));

		// Size of message content.
		int start = 4, end = charCount - 1;
		char strlen[end - start + 1];
		memcpy(strlen, &buff[start], end - start);
		strlen[end - start] = '\0';

		// Format error if pipe symbol is not at end. 
		if (buff[charCount - 1] != '|')
		{
			printf("%c\n",buff[charCount]);
			return buff;
		}

		do {
			read(fd, &buff[charCount], 1);
			charCount++;
		} while(buff[charCount - 1] != '|');

	} 
	else if (buff[0] == 'E' 
		&& buff[1] == 'R' 
		&& buff[2] == 'R' 
		&& buff[3]=='|')
	{
		do {
			read(fd, &buff[charCount], 1);
			charCount++;
		} while(buff[charCount - 1] != '|');
	} 
	
	buff[charCount] = '\0';
	return buff;
}
/*-----------------------------------*/
void writer(int fd, char* buff, int len) 
{
	int curr = 0;
	while (curr < len) 
	{
		int next = write(fd, buff + curr, len - curr);
		if (next == -1) 
		{
			fprintf(stderr, "%s", strerror(errno));
			exit(EXIT_FAILURE);
		}
		curr += next;
	}
}
/*-----------------------------------*/
// Return 0 for valid message, 1 for content error,
// 2 for length error, 3 for format error, 4 for ERR code
int checkResponse1(char* str1)
{
	if (processMsg(str1) == 1)
		return 4;
	if (processMsg(str1) == 2)
		return 3;

	// Format correct, checking content and length.
	char contentLen[10], content[256];
	int i = 4, j = 0;

	while (isdigit(str1[i]))
	{
		contentLen[i - 4] = str1[i];
		i++;
	}
	contentLen[i-4] = '\0';
	++i;

	// Skip over pipe symbols.
	while(str1[i] != '|'){
		content[j++] = str1[i++];
	}
	content[j] = '\0';
	if (strcmp("Who's there?", content))
		return 1;
	
	// Verify length.
	if ((int) strlen(content) != atoi(contentLen))
		return 2;

	// Message 1 is valid.
	return 0;
}
/*-----------------------------------*/
// Return 0 for valid message, 1 for content error,
// 2 for length error, 3 for format error, 4 for ERR code
int checkResponse2(char* str3, char* setup)
{
	if (processMsg(str3) == 1)
		return 4;
	if (processMsg(str3) == 2)
		return 3;

	char contentLen[10], content[256];
	int i = 4, j = 0;

	while (isdigit(str3[i]))
	{
		contentLen[i - 4] = str3[i];
		++i;
	}
	contentLen[i - 4] = '\0';
	++i;

	while (str3[i] != '|')
		content[j++] = str3[i++];
	content[j] = '\0';

	// Verify content is properly formatted.
	char* setupLine = malloc(sizeof(char) * strlen(setup) + 7);
	strcpy(setupLine, setup);
	strcat(setupLine, ", who?");

	if (strcmp(setupLine, content))
	{
		free(setupLine);
		return 1;
	}
	free(setupLine);

	// Verify length is proper.
	if (strlen(content) != atoi(contentLen))
		return 2;

	// Message 3 is valid.
	return 0;
}
/*-----------------------------------*/
// Return 0 for valid message, 1 for content error,
// 2 for length error, 3 for format error, 4 for ERR code
int checkResponse3(char* str5)
{
	if (processMsg(str5) == 1)
		return 4;
	if (processMsg(str5) == 2)
		return 3;

	// Format correct, check content and length
	char contentLen[10], content[256];
	int i = 4, j = 0;

	while (isdigit(str5[i]))
	{
		contentLen[i - 4] = str5[i];
		i++;
	}
	contentLen[i - 4] = '\0';
	i++;

	while(str5[i] != '|')
		content[j++] = str5[i++];
	content[j] = '\0';

	if (strlen(content) != atoi(contentLen))
		return 2;

	j = 0;
	while (j < atoi(contentLen) - 1)
	{
		if (!isalpha(content[j])) 
			return 1;
		j++;
	}

	if (!ispunct(content[j])) 
		return 1;

	// Message 5 is valid.
	return 0;
}
/*-----------------------------------*/
// Print appropriate error message to console.
void printErrorMsg(char* errMsg)
{
	// Possible errors for Message 0.
	if (!strcmp(errMsg, "ERR|M0CT|"))
		printf("M0CT - message 0 content was not correct "
			"(i.e. should be “Knock, knock.”)\n");

	else if (!strcmp(errMsg, "ERR|M0LN|"))
		printf("M0LN - message 0 length value was incorrect "
			"(i.e. should be 13 characters long)\n");

	else if (!strcmp(errMsg, "ERR|M0FT|"))
	{
		printf("M0FT - message 0 format was broken "
			"(did not include a message type, missing or too many '|')\n");
		return;
	}

	// Possible errors for Message 1.
	if (!strcmp(errMsg, "ERR|M1CT|"))
		printf("M1CT - message 1 content was not correct "
			"(i.e. should be “Who's there?”)\n");

	else if (!strcmp(errMsg, "ERR|M1LN|"))
		printf("M1LN - message 1 length value was incorrect "
			"(i.e. should be 12 characters long)\n");

	else if (!strcmp(errMsg, "ERR|M1FT|"))
	{
		printf("M1FT - message 1 format was broken "
			"(did not include a message type, missing or too many '|')\n");
		return;
	}

	// Possible errors for Message 2.
	if (!strcmp(errMsg, "ERR|M2CT|"))
		printf("M2CT - message 2 content was not correct "
			"(i.e. missing punctuation)\n");

	else if (!strcmp(errMsg, "ERR|M2LN|"))
		printf("M2LN - message 2 length value was incorrect "
			"(i.e. should be the length of the message)\n");

	else if (!strcmp(errMsg, "ERR|M2FT|"))
	{
		printf("M2FT - message 2 format was broken "
			"(did not include a message type, missing or too many '|')\n");
		return;
	}
	
	// Possible errors for Message 3.
	if (!strcmp(errMsg, "ERR|M3CT|"))
		printf("M3CT - message 3 content was not correct "
			"(i.e. should contain message 2 with “, who?” tacked on)\n");

	else if (!strcmp(errMsg, "ERR|M3LN|"))
		printf("M3LN - message 3 length value was incorrect "
			"(i.e. should be M2 length plus six)\n");

	else if (!strcmp(errMsg, "ERR|M3FT|"))
	{
		printf("M3FT - message 3 format was broken "
			"(did not include a message type, missing or too many '|')\n");
		return;
	}

	// Possible errors for Message 4.
	if (!strcmp(errMsg, "ERR|M4CT|"))
		printf("M4CT - message 4 content was not correct "
			"(i.e. missing punctuation)\n");

	else if (!strcmp(errMsg, "ERR|M4LN|"))
		printf("M4LN - message 4 length value was incorrect "
			"(i.e. should be the length of the message)\n");

	else if (!strcmp(errMsg, "ERR|M4FT|"))
	{
		printf("M4FT - message 4 format was broken "
			"(did not include a message type, missing or too many '|')\n");
		return;
	}

	// Possible errors for Message 5.
	if (!strcmp(errMsg, "ERR|M5CT|"))
		printf("M5CT - message 5 content was not correct "
			"(i.e. missing punctuation)\n");

	else if (!strcmp(errMsg, "ERR|M5LN|"))
		printf("M5LN - message 5 length value was incorrect "
			"(i.e. should be the length of the message)\n");

	else if (!strcmp(errMsg, "ERR|M5FT|"))
	{
		printf("M5FT - message 5 format was broken "
			"(did not include a message type, missing or too many '|')\n");
		return;
	}
}
/*-----------------------------------*/
// Close current socket, wait for new connection.
void exitSocketOnErr(char* errMsg)
{
	printErrorMsg(errMsg);
	close(currentConn->fd);
	free(currentConn);
}
/*-----------------------------------*/
// Send error message to client, exit current session.
/*
* NOTE: I planned to use itoa() to convert msgNum to a 
* string, so that I could concatenate 1, 3, or 5 into
* "ERR|M(msgNum)(CT || LN || FT)|", but itoa() isn't
* standard, unlike atoi().
*/
void sendErr(int errCode, int msgNum, int fd)
{
	char* message = "";

	// Invalid content.
	if (errCode == 1)
	{
		if (msgNum == 1) message = "ERR|M1CT|";
		else if (msgNum == 3) message = "ERR|M3CT|";
		else if (msgNum == 5) message = "ERR|M5CT|";
	}

	// Invalid length.
	else if (errCode == 2)
	{
		if (msgNum == 1) message = "ERR|M1LN|";
		else if (msgNum == 3) message = "ERR|M3LN|";
		else if (msgNum == 5) message = "ERR|M5LN|";
	}

	// Invalid formatting.
	else
	{
		if (msgNum == 1) message = "ERR|M1FT|";
		else if (msgNum == 3) message = "ERR|M3FT|";
		else if (msgNum == 5) message = "ERR|M5FT|";
	}
	
	writer(fd, message, strlen(message));
	exitSocketOnErr(message);
}
/*-----------------------------------*/
// Error checking for message, returns:
// 0 for valid REG, 1 for valid ERR,
// 2 for improper message formatting.
int processMsg(char* message)
{
	int pipes = 0, i = 0, len = strlen(message);
	int validWord = 0, validNum = 0;

	if (message[0] == 'R'
		&& message[1] == 'E'
		 && message[2] == 'G')
	{
		i = 3;
	    while (i < len)
	    {
			if (message[i] == '|')
			   pipes++;

			if (pipes == 1)
			{
				if (isdigit(message[i]))
					validNum = 1;

				else if (!isdigit(message[i]))
					validNum = 0;
			}

			else if (pipes == 2)
				if (message[i+1]!= '|' && isalpha(message[i+1]))
					validWord = 1;

			if (pipes == 3)
				break;
			i++;
		}

		if (validWord && validNum && pipes == 3)
			return 0;
	}

	else if (message[0]== 'E' 
			 && message[1] == 'R' 
			 && message[2] == 'R')
	{
		if (message[3]=='|')
			if (message[4] == 'M')
				if (isdigit(message[5]))
					if ((message[6] == 'C' && message[7] == 'T') 
					 || (message[6] == 'L' && message[7] == 'N')
					 || (message[6] == 'F' && message[7] == 'T'))
						// All conditions met for ERR.
						return 1;
	}

	// One or more conditions not met, return 2 for improper format.
	return 2;
}
/*-----------------------------------*/
// Exchange messages with connected client.
void echo(connection* arg)
{
	char localhost[100], port[10];
	connection* conn = (connection*) arg;
	int error;
	
	// Get hostname and port of remote host.
	error = getnameinfo((sockaddr*) &conn->addr, conn->addr_len,
				localhost, 100, port, 10, NI_NUMERICSERV);
	
	if (error)
	{
		fprintf(stderr, "getnameinfo(): %s", gai_strerror(error));
		close(conn->fd);
		return;
	}

	int err = 0;
	// Send line 0.
	char* str0 = "REG|13|Knock, knock.|";
	writer(conn->fd, str0, strlen(str0));
	printf("Server Sent... %s\n", str0);

	// Read line 1 from client.
	char* str1 = readMessage(conn->fd);
	printf("From Client... %s\n", str1);
	// Validate line 1.
	err = checkResponse1(str1);
	if (err > 0)
	{
		if (err == 4)
		{
			exitSocketOnErr(str1);
			return;
		}
		
		free(str1);
		sendErr(err, 1, conn->fd);
		return;
	}
	free(str1);

	// Send line 2.
	char* str2 = "REG|9|Dijkstra.|", *setup = "Dijkstra";
	writer(conn->fd, str2, strlen(str2));
	printf("Server Sent... %s\n", str2);
	
	// Read and validate line 3.
	char* str3 = readMessage(conn->fd);
	printf("From Client... %s\n", str3);
	err = checkResponse2(str3, setup);
	if (err > 0) 
	{
		if (err == 4)
		{
			exitSocketOnErr(str3);
			return;
		}

		free(str3);
		sendErr(err, 3, conn->fd);
		return;
	}
	free(str3);

	// Send line 4.
	char* str4 = "REG|50|That path was taking "
		"too long, so I let myself in.|";
	write(conn->fd, str4, strlen(str4));
	printf("Server Sent... %s\n", str4);

	// Read and validate line 5.
	char* str5 = readMessage(conn->fd);
	printf("From Client... %s\n", str5);
	err = checkResponse3(str5);
	if (err > 0) 
	{
		if (err == 4)
		{
			exitSocketOnErr(str5);
			return;
		}

		free(str5);
		sendErr(err, 5, conn->fd);
		return;
	}

	free(str5);
	close(conn->fd);
	free(conn);
}
/*-----------------------------------*/