#ifndef __WHATEVER_H__
#define __WHATEVER_H__

#include <iostream>
#include <cstdio>
#include <ws2tcpip.h>
#include <winsock2.h>
#include <string>
#include <sstream>

#pragma comment(lib, "Ws2_32.lib")
// #pragma comment (lib, "Mswsock.lib")
// #pragma comment (lib, "AdvApi32.lib")

#define DEFAULT_BUFLEN 2048
#define DEFAULT_RESPONSE_LEN 16384
#define DEFAULT_PORT "22333"
#define HTTP_PORT "80"
#define BAD_REQUEST -1

#endif
