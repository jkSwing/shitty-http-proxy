#include <iostream>
#include <cstdio>
#include <ws2tcpip.h>
#include <winsock2.h>

#pragma comment(lib, "Ws2_32.lib")
// #pragma comment (lib, "Mswsock.lib")
// #pragma comment (lib, "AdvApi32.lib")

#define DEFAULT_BUFLEN 512
#define DEFAULT_PORT "22333"


int get_line(SOCKET s, char* buff, int len)
{
	char c = '\0';
	int i = 0;
	while (i < len - 1 && c != '\n')
	{
		int iResult = recv(s, &c, 1, 0);
		if (iResult > 0)
		{
			if (c == '\r')
			{
				buff[i++] = '\r';
				iResult = recv(s, &c, 1, MSG_PEEK);
				// read the \n and break the loop
				if (iResult > 0 && c == '\n')
				{
					recv(s, &c, 1, 0);
				}
				// read null or sth else, break the loop
				else
				{
					c = '\n';
				}
			}
		}
		// cannot read, break the loop 
		else
		{
			c = '\n';
		}
		buff[i++] = c;
	}
	return i;
}

void fatal_error(const char *error_msg, SOCKET socket = INVALID_SOCKET, struct addrinfo *result = NULL)
{
	printf("fatal error: %s with error %ld\n", error_msg, WSAGetLastError());
	if (result != NULL) freeaddrinfo(result);
	if (socket != INVALID_SOCKET) closesocket(socket);
	WSACleanup();
	system("pause");
	exit(1);
}

SOCKET open_listen_socket(const char *port)
{
	int iResult;
	struct addrinfo hints;

	ZeroMemory(&hints, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;
	hints.ai_flags = AI_PASSIVE;

	// Resolve the server address and port
	struct addrinfo *result = NULL;
	iResult = getaddrinfo(NULL, DEFAULT_PORT, &hints, &result);
	if (iResult != 0)
	{
		fatal_error("getaddrinfo failed");
	}

	SOCKET ListenSocket = INVALID_SOCKET;
	for (struct addrinfo *p = result; p != NULL; p->ai_next)
	{
		// Create a SOCKET for connecting to server
		ListenSocket = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
		if (ListenSocket == INVALID_SOCKET) {
			printf("socket failed with error: %ld. trying next one\n", WSAGetLastError());
			continue;
		}

		// Setup the TCP listening socket
		iResult = bind(ListenSocket, p->ai_addr, (int)p->ai_addrlen);
		if (iResult == SOCKET_ERROR) {
			printf("bind failed with error: %d. trying next one\n", WSAGetLastError());
			closesocket(ListenSocket);
			continue;
		}
		break;
	}
	if (ListenSocket == INVALID_SOCKET)
	{
		fatal_error("open listening socket failed", INVALID_SOCKET, result);
	}

	iResult = listen(ListenSocket, SOMAXCONN);
	if (iResult == SOCKET_ERROR) {
		fatal_error("listen failed", ListenSocket, result);
	}

	freeaddrinfo(result);

	printf("succeeded: %u\n", ListenSocket);
	return ListenSocket;
}


int main(int argc, char **argv)
{
	WSADATA wsaData;
	int iResult;

	// Initialize Winsock
	iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (iResult != 0) {
		printf("WSAStartup failed with error: %d\n", iResult);
		return 1;
	}

	SOCKET ListenSocket = open_listen_socket(DEFAULT_PORT);

	while (true)
	{
		// Accept a client socket
		SOCKET ClientSocket = accept(ListenSocket, NULL, NULL);
		if (ClientSocket == INVALID_SOCKET) {
			fatal_error("accept failed", ListenSocket);
		}

		// Receive until the peer shuts down the connection
		char recvbuf[DEFAULT_BUFLEN] = { 0 };
		int recvbuflen = DEFAULT_BUFLEN;
		int readLen = 0;
		iResult = 0;

		// break if read an empty line
		do
		{
			readLen += iResult;
			iResult = get_line(ClientSocket, recvbuf + readLen, DEFAULT_BUFLEN - readLen);
		} while (iResult > 0 && strcmp(recvbuf + readLen, "\r\n") != 0);

		printf("%s", recvbuf);
		

		// todo: connect to the destination

		// todo: send the data back to client
		// send a constant content
		char buff[] = "HTTP/1.0 200 OK\r\n"
			"Server: Apache Tomcat/5.0.12\r\n"
			"Content-Type: text/html;charset=UTF-8\r\n"
			"Content-Length: 17\r\n"
			"\r\n"
			"<h1>REPLIED</h1>";
		iResult = send(ClientSocket, buff, sizeof(buff), 0);
		if (iResult < 0)
		{
			fatal_error("send failed", ClientSocket);
		}

		// shutdown the connection since we're done
		iResult = shutdown(ClientSocket, SD_SEND);
		if (iResult == SOCKET_ERROR) {
			fatal_error("shutdown failed", ClientSocket);
		}

		closesocket(ClientSocket);
	}

	// cleanup
	closesocket(ListenSocket);
	WSACleanup();

	return 0;
}