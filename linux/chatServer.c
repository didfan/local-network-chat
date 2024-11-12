#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>

#define PORT 8080
#define IP "127.0.0.1"
#define MAX_QUEUE_SIZE 3
#define MAX_USERS_COUNT 3

#define MESSAGE_INPUT_USERNAME "Enter your username:\n"
#define DISCONNECT_COMMAND "/disconnect"

#define SIZE_BUFFER 512
#define MAX_NAME_LENGTH 30

int CreateListenSocket(int* sock, struct sockaddr_in* addr, int maxQueueSize)
{
	int isSuccess;
	int optionValue = 1;

	*sock = socket(AF_INET, SOCK_STREAM, 0);
	if(*sock == -1)
	{
		perror("socket");
		return -1;
	}

	setsockopt(*sock, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &optionValue, sizeof(optionValue));
		
	addr->sin_family = AF_INET;
	addr->sin_port = htons(PORT);
	addr->sin_addr.s_addr = inet_addr(IP);
	
	isSuccess = bind(*sock, (struct sockaddr*)addr, sizeof(*addr));
	if(isSuccess == -1)
	{
		perror("bind");
		return -2;
	}

	listen(*sock, MAX_QUEUE_SIZE);

	return 0;
}

void AddNewUser(int* sock, int currentSocket, int* usersCount)
{
	*sock = currentSocket;
	(*usersCount)++;		
	write(*sock, MESSAGE_INPUT_USERNAME, sizeof(MESSAGE_INPUT_USERNAME));
}

void DisconnectUser(int* sock, int* authorizedSocket, char* userName, int* usersCount)
{
	close(*sock);				
	*sock = 0;
	*authorizedSocket = 0;
	(*usersCount)--;
	
	memset(userName, 0, strlen(userName));
}

void DisconnectUserWithMessage(int* sock, int* authorizedSocket, char* userName, int* usersCount, char* buffer)
{
	memset(buffer, 0, strlen(buffer));
	sprintf(buffer, "User %s disconnected\n", userName);
	DisconnectUser(sock, authorizedSocket, userName, usersCount);
}

int CheckUserDisconnection(char* buffer)
{
	return (strncmp(buffer, DISCONNECT_COMMAND, strlen(DISCONNECT_COMMAND)) == 0);
}

void SendOutMessage(int currentUser, int usersCount, int* authorizedSockets, int* usersSocket, char* buffer)
{
	for(int j = 0; j < usersCount; j++)
		if(currentUser != j && authorizedSockets[j])
			write(usersSocket[j], buffer, strlen(buffer));	
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
	int isSuccess = 0;

	int sock;
	struct sockaddr_in addr;
	isSuccess = CreateListenSocket(&sock, &addr, MAX_QUEUE_SIZE);
	if(isSuccess < 0)
		return 1;

	int authorizedSockets[MAX_USERS_COUNT] = {0};
	int usersSocket[MAX_USERS_COUNT] = {0};
	int currentSocket = 0;

	char* userNames[MAX_USERS_COUNT] = {NULL};
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
						AddNewUser(&usersSocket[i], currentSocket, &usersCount);
					break;
				}

		for(int i = 0; i < usersCount; i++)
			if(FD_ISSET(usersSocket[i], &readfds))
			{
				memset(buffer, 0, strlen(buffer));
				isSuccess = read(usersSocket[i], buffer, sizeof(buffer));							
				if(isSuccess < 0)
					continue;
				else if(isSuccess == 0)
				{
					if(authorizedSockets[i] != 0)
					{
						DisconnectUserWithMessage(&(usersSocket[i]), &(authorizedSockets[i]), userNames[i], &usersCount, buffer);

						SendOutMessage(i, usersCount, authorizedSockets, usersSocket, buffer);
						printf("%s", buffer);
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
	return 0;
}
