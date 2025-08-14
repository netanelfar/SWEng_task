#include <windows.h>
#include <process.h>
#include <stdio.h>
#include <string.h>
#include "ring_buffer.h"
#include "reader.h"
#include "tcp.h"
#include "packer.h"

#define RB_CAPACITY (256*1024)

#define BAUD 115200
#

volatile LONG g_running = 1;
static ByteRing g_rb;

int main(int argc, char** argv) {
    // parse args
    ReaderConfig cfg = {0};
    cfg.baud = BAUD;
    for (int i=1;i<argc;++i){
        if (!strcmp(argv[i],"--com") && i+1<argc){ cfg.use_serial=true; strncpy(cfg.com_name, argv[++i], sizeof cfg.com_name-1); }
        else if (!strcmp(argv[i],"--baud") && i+1<argc){ cfg.baud = (DWORD)strtoul(argv[++i], NULL, 10); }
        else { printf("Usage: sender.exe [--com COMx] [--baud 115200]\n"); return 0; }
    }

    if (!rb_init(&g_rb, RB_CAPACITY)) { fprintf(stderr,"rb_init failed\n"); return 1; }
    if (!tcp_init()) { fprintf(stderr,"WSAStartup failed\n"); rb_free(&g_rb); return 1; }

    SOCKET sock = tcp_connect("127.0.0.1", 5555);
    if (sock == INVALID_SOCKET) { tcp_cleanup(); rb_free(&g_rb); return 1; }
    printf("[main] connected to receiver\n");

    Reader reader;
    if (!reader_start(&reader, &cfg, &g_rb, &g_running)) {
        closesocket(sock); tcp_cleanup(); rb_free(&g_rb); return 1;
    }

    PackerArgs pa = { sock, &g_rb };
    HANDLE hPacker = (HANDLE)_beginthreadex(NULL, 0, packer_thread, &pa, 0, NULL);

    puts("Sender running. Press ENTER to stop.");
    getchar();

    InterlockedExchange(&g_running, 0);
    rb_close(&g_rb);
    reader_join(&reader);
    WaitForSingleObject(hPacker, INFINITE);
    CloseHandle(hPacker);
    closesocket(sock);
    tcp_cleanup();
    rb_free(&g_rb);
    puts("Sender stopped.");
    return 0;
}
