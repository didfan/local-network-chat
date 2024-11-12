#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <conio.h>
#include <winsock2.h>
#include <ws2tcpip.h>

#define SIZE_BUFFER 512
#define SIZE_PARAMETERS_BUFFER 32

#define MESSAGE_SERVER_FULL "Server is full\n"
#define NUMBER_DATA_WAIT_CYCLES 1000

void QuerySocketData(ULONG* ip, USHORT* port)
{
	char buffer[SIZE_PARAMETERS_BUFFER];	
	int bufferInt;
	int isSuccess = 0;

	while(1)
	{
		puts("Enter the ip address of the server: ");
		fgets(buffer, sizeof(buffer), stdin);	
		*ip = inet_addr(buffer);	

		if(*ip == (ULONG)(-1))
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

int CreateSocket(SOCKET* sock, struct sockaddr_in* serverAddr)
{
	*sock = socket(AF_INET, SOCK_STREAM, 0);
	if(sock < 0)
	{
		perror("socket");
		return -1;
	}
	return 0;
}

int ProcessServerMessage(SOCKET sock, char* buffer, int bufferSize)
{
	memset(buffer, 0, strlen(buffer));
	int result = recv(sock, buffer, bufferSize, 0);
	if(result != -1)
		printf("%s", buffer);
	return result;
}

int ProcessNonBlockingInput(SOCKET sock, char* buffer, int* bufferUsed, int bufferSize)
{
	if(_kbhit())
	{
		buffer[*bufferUsed] = _getch();
		(*bufferUsed)++;

		if(buffer[*bufferUsed-1] != '\r')
			_putch(buffer[*bufferUsed-1]);
		else 
			_putch('\n');

		if(buffer[*bufferUsed-1] == '\r' || *bufferUsed == bufferSize)
		{
			buffer[*bufferUsed-1] = '\n';
			send(sock, buffer, *bufferUsed, 0);
			memset(buffer, 0, *bufferUsed);
			*bufferUsed = 0;
			return 1;
		} 
	}
	return 0;
}

int ProcessUsernameRequest(SOCKET sock, char* buffer, int bufferSize)
{
	int bufferUsed = 0;
	int isSuccess = 0;
	
	//Wait data
	for(int i = 0; i < NUMBER_DATA_WAIT_CYCLES; i++)
	{
		isSuccess = ProcessServerMessage(sock, buffer, bufferSize);
		if(isSuccess != -1)
			break;
	}

	if(isSuccess == 0 || isSuccess == -1)
		return 0;

	while(!isSuccess)
		isSuccess = ProcessNonBlockingInput(sock, buffer, &bufferUsed, bufferSize);

	return 1;
}

int main()
{
	WSADATA wsaData;
	SOCKET sock;
	struct sockaddr_in serverAddr;
	
	char buffer[SIZE_BUFFER];

	int isSuccess = 0;

	isSuccess = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if(isSuccess != NO_ERROR)
	{
		perror("WSAStartup");
		return 1;
	}
	
	isSuccess = CreateSocket(&sock, &serverAddr);
	if(isSuccess < 0)
		return 2;
		
	serverAddr.sin_family = AF_INET;
	QuerySocketData(&serverAddr.sin_addr.s_addr, &serverAddr.sin_port);		

	isSuccess = connect(sock, (SOCKADDR*)&serverAddr, sizeof(serverAddr));
	if(isSuccess < 0)
	{
		perror("connect");
		return 3;
	}
	
	u_long socketMode = 1;
	isSuccess = ioctlsocket(sock, FIONBIO, &socketMode);
	if(isSuccess != NO_ERROR)
	{
		perror("iocltsocket");
		return 3;
	}

	isSuccess = ProcessUsernameRequest(sock, buffer, sizeof(buffer));
	if(isSuccess == 0)
	{
		printf("%s", MESSAGE_SERVER_FULL);
		printf("Press any symbol to close this window\n");
		getchar();
		return 0;
	}

	char readBuffer[SIZE_BUFFER];
	int readBufferUsed = 0;
	memset(readBuffer, 0, sizeof(readBuffer));

	while(1)
	{
		ProcessServerMessage(sock, buffer, sizeof(buffer));
		ProcessNonBlockingInput(sock, readBuffer, &readBufferUsed, sizeof(readBuffer));
	}
	WSACleanup();
	return 0;
}
