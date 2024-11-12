#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define SIZE_BUFFER 512
#define SIZE_PARAMETERS_BUFFER 32

#define MESSAGE_SERVER_FULL "Server is full\n"
#define MESSAGE_SERVER_DISCONNECT "Server disconnected\n"

void QuerySocketData(in_addr_t* ip, in_port_t* port)
{
	char buffer[SIZE_PARAMETERS_BUFFER];	
	int bufferInt;
	int isSuccess = 0;

	while(1)
	{
		puts("Enter the ip address of the server: ");
		fgets(buffer, sizeof(buffer), stdin);	
		*ip = inet_addr(buffer);	

		if(*ip == (in_addr_t)(-1))
			puts("Input error! Please try again: ");
		else 
			break;
	}
	
	while(1)
	{
		puts("Enter the port of the server: ");
		isSuccess = scanf("%d", &bufferInt);
		*port = htons(bufferInt);

		if(isSuccess == 0)
			puts("Input error! Please try again: ");
		else
			break;
	}
	getchar();
}

int CreateSocket(int* sock, struct sockaddr_in* serverAddr)
{
	*sock = socket(AF_INET, SOCK_STREAM, 0);
	if(sock < 0)
	{
		perror("socket");
		return -1;
	}
	return 0;
}

int CheckServerFull(int sock)
{
	fd_set readfds;
	struct timeval timeout;
	int result;

	FD_SET(sock, &readfds);
	
	timeout.tv_sec = 0;
	timeout.tv_usec = 500000;

	result = select(sock+1, &readfds, NULL, NULL, &timeout);
	return (result == 0 ? 0 : 1);
}

void ProcessServerMessage(int sock, char* buffer, int bufferSize)
{
	memset(buffer, 0, strlen(buffer));
	read(sock, buffer, bufferSize);
	printf("%s", buffer);
}

void ProcessInput(int sock, char* buffer, int bufferSize)
{
	memset(buffer, 0, strlen(buffer));
	read(0, buffer, bufferSize);
	write(sock, buffer, strlen(buffer));
}

void ProcessUsernameRequest(int sock, char* buffer, int bufferSize)
{
	ProcessServerMessage(sock, buffer, bufferSize);
	ProcessInput(sock, buffer, bufferSize);
}

int main()
{
	int sock;
	struct sockaddr_in serverAddr;
	
	char buffer[SIZE_BUFFER];

	int isSuccess = 0;

	
	isSuccess = CreateSocket(&sock, &serverAddr);
	if(isSuccess < 0)
		return 1;
		
	serverAddr.sin_family = AF_INET;
	QuerySocketData(&serverAddr.sin_addr.s_addr, &serverAddr.sin_port);		

	isSuccess = connect(sock, (struct sockaddr*)&serverAddr, sizeof(serverAddr));
	if(isSuccess < 0)
	{
		perror("connect");
		return 2;
	}

	if(!CheckServerFull(sock))
	{
		printf("%s", MESSAGE_SERVER_FULL);
		return 3;
	}
	
	ProcessUsernameRequest(sock, buffer, sizeof(buffer));

	int flags = fcntl(0, F_GETFL, 0);	
	fcntl(0, F_SETFL, flags | O_NONBLOCK);

	fd_set readfds;
	struct timeval timeout;	
	int maxDescriptor = sock;
	int result;

	while(1)
	{
		FD_ZERO(&readfds);
		FD_SET(sock, &readfds);
		FD_SET(0, &readfds);

		timeout.tv_sec = 1;
		timeout.tv_usec = 0;

		result = select(maxDescriptor+1, &readfds, NULL, NULL, &timeout);
		if(result == 0 || result == -1)
			continue;

		if(FD_ISSET(sock, &readfds))
			ProcessServerMessage(sock, buffer, sizeof(buffer));

		if(FD_ISSET(0, &readfds))
			ProcessInput(sock, buffer, sizeof(buffer));
	}
	return 0;
}
