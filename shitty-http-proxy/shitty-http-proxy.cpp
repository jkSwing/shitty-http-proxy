#include <iostream>
#include <cstdio>
#include <ws2tcpip.h>
#include <winsock2.h>

#pragma comment(lib, "Ws2_32.lib")
// #pragma comment (lib, "Mswsock.lib")
// #pragma comment (lib, "AdvApi32.lib")

#define DEFAULT_BUFLEN 512
#define DEFAULT_PORT "443"

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
		char recvbuf[DEFAULT_BUFLEN];
		int recvbuflen = DEFAULT_BUFLEN;
		do {

			iResult = recv(ClientSocket, recvbuf, recvbuflen, 0);
			if (iResult > 0) {
				printf("%d bytes received\n", iResult);
				printf("raw data:\n%s\n", recvbuf);
				fflush(stdout);
			}
			else if (iResult == 0)
				printf("Connection closing...\n");
			else {
				fatal_error("recv failed", ClientSocket);
			}

		} while (iResult > 0);

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