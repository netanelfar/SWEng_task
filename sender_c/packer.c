#include "packer.h"
#include <process.h>
#include <stdio.h>


extern volatile LONG g_running; // declared in sender.c

unsigned __stdcall packer_thread(void* arg) {
    PackerArgs* pa = (PackerArgs*)arg;
    uint8_t frame[FRAME_SIZE];
    unsigned count = 0;

    printf("[packer] started\n");
    while (InterlockedCompareExchange(&g_running, 1, 1) == 1) {
        rb_pop_exact(pa->rb, frame, FRAME_SIZE); // blocks until 100 ready
        if (!tcp_send_all(pa->sock, frame, FRAME_SIZE)) {
            fprintf(stderr, "[packer] send failed\n");
            break;
        }
        if ((++count % 500) == 0) printf("[packer] sent %u frames\n", count);
    }
    printf("[packer] exiting\n");
    return 0;
}
