#include "tcp.h"
#include <stdio.h>

bool tcp_init(void) {
    WSADATA w; return (WSAStartup(MAKEWORD(2,2), &w) == 0);
}
void tcp_cleanup(void) { WSACleanup(); }

SOCKET tcp_connect(const char* ip, unsigned short port) {
    SOCKET s = socket(AF_INET, SOCK_STREAM, 0);
    if (s == INVALID_SOCKET) { fprintf(stderr, "[tcp] socket failed\n"); return INVALID_SOCKET; }
    struct sockaddr_in a; a.sin_family = AF_INET; a.sin_port = htons(port);
    if (inet_pton(AF_INET, ip, &a.sin_addr) != 1) { fprintf(stderr, "[tcp] bad ip\n"); closesocket(s); return INVALID_SOCKET; }
    if (connect(s, (struct sockaddr*)&a, sizeof a) < 0) { fprintf(stderr, "[tcp] connect failed\n"); closesocket(s); return INVALID_SOCKET; }
    return s;
}

bool tcp_send_all(SOCKET s, const void* buf, size_t len) {
    const char* p = (const char*)buf; size_t off = 0;
    while (off < len) {
        int n = send(s, p + off, (int)(len - off), 0);
        if (n <= 0) return false;
        off += (size_t)n;
    }
    return true;
}
