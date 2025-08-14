#pragma once
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdbool.h>
#pragma comment(lib,"ws2_32.lib")

bool tcp_init(void);
void tcp_cleanup(void);

// Connect to ip:port (e.g., "127.0.0.1", 5555). Returns INVALID_SOCKET on fail.
SOCKET tcp_connect(const char* ip, unsigned short port);

// Send exactly len bytes (loops until done). Returns false on error.
bool tcp_send_all(SOCKET s, const void* buf, size_t len);
