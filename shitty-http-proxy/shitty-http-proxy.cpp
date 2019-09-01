#include <shitty-http-proxy.h>

void fatal_error(const char *error_msg, SOCKET socket = INVALID_SOCKET, struct addrinfo *result = NULL)
{
	printf("fatal error: %s with error %ld\n", error_msg, WSAGetLastError());
	if (result != NULL) freeaddrinfo(result);
	if (socket != INVALID_SOCKET) closesocket(socket);
	WSACleanup();
	system("pause");
	exit(1);
}

// buffer store the data received from 'socket'
struct ShitBuffer
{
	char buffer[DEFAULT_BUFLEN] = {0};
	char *cur = buffer, *end = buffer;
	SOCKET socket;

	ShitBuffer(SOCKET socket): socket(socket) {}
	int read(char *user_buffer, int len, int flags)
	{
		int read_cnt = 0;
		while (read_cnt < len)
		{
			if (cur == end)  // if no new data
			{
				int iResult = recv(socket, buffer, DEFAULT_BUFLEN, 0);
				if (iResult == SOCKET_ERROR) return SOCKET_ERROR;
				if (iResult == 0) return read_cnt;

				cur = buffer; end = buffer + iResult;
			}
			user_buffer[read_cnt++] = *cur;
			if (!(flags & MSG_PEEK)) ++cur;
		}
		return read_cnt;
	}
	int get_line(char *buff, int len)  // maybe need refactor
	{
		char c = '\0';
		int read_cnt = 0;
		while (read_cnt < len - 1 && c != '\n')
		{
			int iResult = read(&c, 1, 0);
			// cannot read, break the loop 
			if (iResult <= 0)
			{
				if (iResult == SOCKET_ERROR) fatal_error("socket error", socket);
				break;
			}
			buff[read_cnt++] = c;
		}
		buff[read_cnt] = 0;
		return read_cnt;
	}
};

struct Meta
{
	std::string method;
	std::string host;
	std::string path;
	std::string user_agent;
	int content_length = 0;
	std::string content;
};

void parse_line(Meta &meta, char line[])
{
	std::string buf;
	std::istringstream is(std::string{line});
	is >> buf;
	if (buf.empty()) return;
	if (buf.back() == ':') buf.pop_back();

	if (buf == "GET")
	{
		meta.method = buf;
		is >> meta.path;
	}
	else if (buf == "Host")
	{
		is >> meta.host;
	}
	else if (buf == "User-Agent")
	{
		is.get();  // eat white space
		std::getline(is, meta.user_agent);
	}
}

int build_request(char request[], const Meta &meta)
{
	if (meta.method.empty()) return BAD_REQUEST;
	if (meta.path.empty()) return BAD_REQUEST;
	if (meta.host.empty()) return BAD_REQUEST;
	std::string format = "%s %s HTTP/1.0\r\n"
		"Host: %s\r\n"
		"Connection: close\r\n"
		"Proxy-Connection: close\r\n"
		"User-Agent: %s\r\n";
	if (meta.content_length > 0) format += "Content-Length: %d\r\n"
											"\r\n"
											"%s";
	format += "\r\n";
	sprintf_s(request, DEFAULT_BUFLEN, format.c_str(), meta.method.c_str(), meta.path.c_str(),
			meta.host.c_str(), meta.user_agent.c_str(), meta.content_length, meta.content.c_str());
	return 0;
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
		ShitBuffer shit_buffer(ClientSocket);
		Meta meta;
		char recvbuf[DEFAULT_BUFLEN] = { 0 };
		int readLen = 0;

		// break if read an empty line
		do
		{
			iResult = shit_buffer.get_line(recvbuf, DEFAULT_BUFLEN);
			if (iResult <= 0 || strcmp(recvbuf, "\r\n") == 0) break;
			parse_line(meta, recvbuf);
		} while (true);

		char request[DEFAULT_BUFLEN] = {0};
		build_request(request, meta);
		printf("%s", request);
		// printf("%s", recvbuf);
		

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