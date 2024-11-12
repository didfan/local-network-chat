#define WIN32_LEAN_AND_MEAN

#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdio.h>
#include <io.h>
#include <string.h>
#include <stdlib.h>
#include <malloc.h>

#define PORT 8080
#define IP "127.0.0.1"
#define MAX_QUEUE_SIZE 3
#define MAX_USERS_COUNT 3

#define MESSAGE_INPUT_USERNAME "Enter your username:\n"
#define DISCONNECT_COMMAND "/disconnect"

#define SIZE_BUFFER 512
#define MAX_NAME_LENGTH 30

int CreateListenSocket(SOCKET* sock, struct sockaddr_in* addr, int maxQueueSize)
{
	int isSuccess;
	char optionValue = 1;

	*sock = socket(AF_INET, SOCK_STREAM, 0);
	if(*sock == -1)
	{
		perror("socket");
		return -1;
	}

	setsockopt(*sock, SOL_SOCKET, SO_REUSEADDR, &optionValue, sizeof(optionValue));
		
	addr->sin_family = AF_INET;
	addr->sin_port = htons(PORT);
	addr->sin_addr.s_addr = inet_addr(IP);
	
	isSuccess = bind(*sock, (SOCKADDR*)addr, sizeof(*addr));
	if(isSuccess == -1)
	{
		perror("bind");
		return -2;
	}

	listen(*sock, MAX_QUEUE_SIZE);

	return 0;
}

void AcceptNewUser(SOCKET* sock, int currentSocket, int* authorizedSocket, int* usersCount)
{
	*sock = currentSocket;
	(*usersCount)++;		
	send(*sock, MESSAGE_INPUT_USERNAME, sizeof(MESSAGE_INPUT_USERNAME), 0);
}

void DisconnectUser(SOCKET* sock, int* authorizedSocket, char* userName, int* usersCount)
{
	close(*sock);				
	*sock = 0;
	*authorizedSocket = 0;
	(*usersCount)--;

	memset(userName, 0, strlen(userName));
}

void DisconnectUserWithMessage(SOCKET* sock, int* authorizedSocket, char* userName, int* usersCount, char* buffer)
{
	memset(buffer, 0, strlen(buffer));
	sprintf(buffer, "User %s disconnected\n", userName);
	DisconnectUser(sock, authorizedSocket, userName, usersCount);
}

int CheckUserDisconnection(char* buffer)
{
	return (strncmp(buffer, DISCONNECT_COMMAND, strlen(DISCONNECT_COMMAND)) == 0);
}

void SendOutMessage(int currentUser, int usersCount, int* authorizedSockets, SOCKET* usersSocket, char* buffer)
{
	for(int j = 0; j < usersCount; j++)
		if(currentUser != j && authorizedSockets[j])
			send(usersSocket[j], buffer, strlen(buffer), 0);	
}

void ReportNewUser(char* buffer, char* userName, int* authorizedSocket)
{
	buffer[strlen(buffer)-1] = '\0';
	strcpy(userName, buffer);
	*authorizedSocket = 1;
	strcat(buffer, " has joined!\n");
}

void SetMessageOwner(char* buffer, char* userName)
{
	memcpy(buffer + strlen(userName) + 2, buffer, strlen(buffer) + 1);
	memcpy(buffer, userName, strlen(userName) + 2);
	memcpy(buffer + strlen(userName), ": ", 2);
}

int main()
{
	WSADATA wsaData;
	SOCKET sock;
	struct sockaddr_in addr;
	int isSuccess = 0;

	isSuccess = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if(isSuccess != NO_ERROR)
	{
		perror("WSAStartup");
		return 1;
	}

	isSuccess = CreateListenSocket(&sock, &addr, MAX_QUEUE_SIZE);
	if(isSuccess < 0)
		return 2;

	int authorizedSockets[MAX_USERS_COUNT] = {0};
	SOCKET usersSocket[MAX_USERS_COUNT] = {0};
	int currentSocket = 0;

	char* userNames[MAX_USERS_COUNT];
	for(int i = 0; i < MAX_USERS_COUNT; i++)
		userNames[i] = (char*)malloc(MAX_NAME_LENGTH * sizeof(char));

	int usersCount = 0;
	
	int result;
	int maxDescriptor = sock;
	fd_set readfds;
	struct timeval timeout;

	char buffer[SIZE_BUFFER];
	while(1)
	{
		FD_ZERO(&readfds);
		
		FD_SET(sock, &readfds);
		for(int i = 0; i < usersCount; i++)
		{
			FD_SET(usersSocket[i], &readfds);
			if(usersSocket[i] > maxDescriptor)
				maxDescriptor = usersSocket[i];
		}

		timeout.tv_sec = 1;
		timeout.tv_usec = 0;
		
		result = select(maxDescriptor+1, &readfds, NULL, NULL, &timeout);
		if(result == -1 || result == 0)
			continue;

		if(FD_ISSET(sock, &readfds))
			for(int i = 0; i <= usersCount; i++)
				if(usersSocket[i] == 0)
				{	
					currentSocket = accept(sock, NULL, NULL);
					
					if(usersCount == MAX_USERS_COUNT)
						close(currentSocket);
					else
						AcceptNewUser(&usersSocket[i], currentSocket, &authorizedSockets[i], &usersCount);
					break;
				}

		for(int i = 0; i < usersCount; i++)
			if(FD_ISSET(usersSocket[i], &readfds))
			{
				memset(buffer, 0, strlen(buffer));
				isSuccess = recv(usersSocket[i], buffer, sizeof(buffer), 0);							
				if(isSuccess <= 0)
				{
					if(authorizedSockets[i] != 0)
					{
						DisconnectUserWithMessage(&(usersSocket[i]), &(authorizedSockets[i]), userNames[i], &usersCount, buffer);

						printf("%s", buffer);
						SendOutMessage(i, usersCount, authorizedSockets, usersSocket, buffer);
					}
					else
						DisconnectUser(&(usersSocket[i]), &(authorizedSockets[i]), userNames[i], &usersCount);
					continue;
				}

				if(authorizedSockets[i] != 0)
				{
					if(CheckUserDisconnection(buffer))
						DisconnectUserWithMessage(&(usersSocket[i]), &(authorizedSockets[i]), userNames[i], &usersCount, buffer);
					else
						SetMessageOwner(buffer, userNames[i]);
				}
				else 
					ReportNewUser(buffer, userNames[i], &(authorizedSockets[i]));				
					
				SendOutMessage(i, usersCount, authorizedSockets, usersSocket, buffer);
				printf("%s", buffer);
			}	
			
	}
	WSACleanup();
	return 0;
}
