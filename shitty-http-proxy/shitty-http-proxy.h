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

#define DEFAULT_BUFLEN 512
#define DEFAULT_PORT "22333"
#define BAD_REQUEST -1

#endif
