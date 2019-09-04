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
struct SockBuffer
{
	char buffer[DEFAULT_BUFLEN] = {0};
	char *cur = buffer, *end = buffer;
	SOCKET socket;

	SockBuffer(SOCKET socket): socket(socket) {}
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

				cur = buffer; 
				end = buffer + iResult;
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

struct HttpMsg
{
	std::string method;
	std::string host;
	std::string path;
	std::string user_agent;
	std::string others;
	int content_length = 0;
	std::string content;
};

void parse_line(HttpMsg &meta, char line[])
{
	std::string buf;
	std::istringstream is(std::string{line});
	is >> buf;
	if (buf.empty()) return;
	if (buf.back() == ':') buf.pop_back();

	if (buf == "GET" || buf == "POST")
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
		// be careful about '\r'!
		if (!meta.user_agent.empty() && meta.user_agent.back() == '\r')
		{
			meta.user_agent.pop_back();
		}
	}
	else if (buf == "Content-Length")
	{
		std::string sLen;
		is >> sLen;
		meta.content_length = std::stoi(sLen);
	}
	else if (buf == "Proxy-Connection" || buf == "Connection")
	{
		return;
	}
	else
	{
		meta.others += line;
	}
}

int build_request(char request[], const HttpMsg &meta)
{
	if (meta.method.empty()) return BAD_REQUEST;
	if (meta.path.empty()) return BAD_REQUEST;
	if (meta.host.empty()) return BAD_REQUEST;
	std::string format = "%s %s HTTP/1.0\r\n"
		"Host: %s\r\n"
		"Connection: close\r\n"
		"Proxy-Connection: close\r\n"
		"User-Agent: %s\r\n";
		// "%s";	// meta.others has "\r\n"
	if (meta.content_length > 0)
	{
		format += "Content-Length: %d\r\n"
				"\r\n"
				"%s";
	}
	format += "\r\n";
	sprintf_s(request, DEFAULT_BUFLEN, format.c_str(), meta.method.c_str(), meta.path.c_str(),
			meta.host.c_str(), meta.user_agent.c_str(), /*meta.others.c_str(),*/ meta.content_length,
			meta.content.c_str());
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

SOCKET connect_to_destination(HttpMsg& meta)
{
	int iResult;
	struct addrinfo hints;

	ZeroMemory(&hints, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;

	struct addrinfo* addrResult = nullptr;

	char desPort[6] = "80";
	// if the port is not the default http port
	if (!meta.host.empty() && !isdigit(meta.host.front()) &isdigit(meta.host.back()))
	{
		char const* portHead = meta.host.c_str() + meta.host.size() - 1;
		int portNumCnt = 0;
		while (isdigit(*portHead))
		{
			--portHead;
			++portNumCnt;
		}
		++portHead;
		strcpy(desPort, portHead);
		// pop the port number
		while (portNumCnt--)
		{
			meta.host.pop_back();
		}
		// pop the ':'
		meta.host.pop_back();
	}

	iResult = getaddrinfo(meta.host.c_str(), desPort, &hints, &addrResult);
	if (iResult != 0)
	{
		fatal_error("getaddrinfo failed");
	}

	SOCKET desSock = INVALID_SOCKET;
	for (struct addrinfo* pInfo = addrResult; pInfo != nullptr; pInfo = pInfo->ai_next)
	{
		// Create a SOCKET for connecting to destination
		desSock = socket(pInfo->ai_family, pInfo->ai_socktype, pInfo->ai_protocol);
		if (desSock == INVALID_SOCKET) {
			printf("socket failed with error: %ld. trying next one\n", WSAGetLastError());
			continue;
		}

		iResult = connect(desSock, pInfo->ai_addr, pInfo->ai_addrlen);
		if (iResult == SOCKET_ERROR)
		{
			fatal_error("connect to destination failed", desSock);
		}
		else
		{
			break;
		}
	}
	return desSock;
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
		SOCKET ClientSocket = accept(ListenSocket, nullptr, nullptr);
		if (ClientSocket == INVALID_SOCKET) {
			fatal_error("accept failed", ListenSocket);
		}

		// Receive until the peer shuts down the connection
		SockBuffer shit_buffer(ClientSocket);
		HttpMsg meta;
		char recvbuf[DEFAULT_BUFLEN] = { 0 };

		// break if read an empty line
		do
		{
			iResult = shit_buffer.get_line(recvbuf, DEFAULT_BUFLEN);
			if (iResult <= 0 || strcmp(recvbuf, "\r\n") == 0) break;
			parse_line(meta, recvbuf);
		} while (true);

		// fill the content (for POST and other method)
		if (meta.content_length > 0)
		{
			int readLen = 0;
			do
			{
				iResult = shit_buffer.read(recvbuf, meta.content_length, 0);
				readLen += iResult;
			} while (readLen < meta.content_length);
			recvbuf[readLen] = '\0';
			meta.content = recvbuf;
		}

		char request[DEFAULT_BUFLEN] = {0};
		iResult = build_request(request, meta);
		if (iResult == BAD_REQUEST)
		{
			goto ShutDownClient;
		}
		printf("===request===\n%s", request);

		
		

		// connect to the destination
		char response[DEFAULT_RESPONSE_LEN] = { 0 };
		SOCKET dstSock = connect_to_destination(meta);
		int readLen = 0;

		iResult = send(dstSock, request, sizeof(request), 0);
		while (true)
		{
			int iResult;
			iResult = recv(dstSock, response + readLen, DEFAULT_RESPONSE_LEN - readLen, 0);
			if (iResult > 0)
			{
				readLen += iResult;
			}
			else
			{
				break;
			}
		}
		printf("===respons===\n%s", response);


		// send the data back to client
		send(ClientSocket, response, sizeof(response), 0);

		// send a constant content
		// char buff[] = "HTTP/1.0 200 OK\r\n"
		// 	"Server: Apache Tomcat/5.0.12\r\n"
		// 	"Content-Type: text/html;charset=UTF-8\r\n"
		// 	"Content-Length: 17\r\n"
		// 	"\r\n"
		// 	"<h1>REPLIED</h1>";
		// iResult = send(ClientSocket, buff, sizeof(buff), 0);
		// if (iResult < 0)
		// {
		// 	fatal_error("send failed", ClientSocket);
		// }

		iResult = shutdown(dstSock, SD_SEND);
		if (iResult == SOCKET_ERROR) {
			fatal_error("shutdown failed", dstSock);
		}
		closesocket(dstSock);

	ShutDownClient:
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