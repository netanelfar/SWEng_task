#include "reader.h"
#include <process.h>
#include <stdio.h>

static unsigned __stdcall reader_thread(void* arg) {
    Reader* r = (Reader*)arg;
    uint8_t buf[2048];

    while (InterlockedCompareExchange(r->running, 1, 1) == 1) {
        size_t n = serial_read_some(&r->serial, buf, sizeof buf);
        if (n > 0) rb_push_bytes(r->rb, buf, n);
        // serial_read_some already sleeps briefly on idle
    }
    return 0;
}

bool reader_start(Reader* r, const ReaderConfig* cfg, ByteRing* rb, volatile LONG* running) {
    ZeroMemory(r, sizeof *r);
    r->rb = rb;
    r->running = running;

    if (!serial_open(&r->serial, cfg)) {
        return false;
    }
    r->thread = (HANDLE)_beginthreadex(NULL, 0, reader_thread, r, 0, NULL);
    if (!r->thread) {
        fprintf(stderr, "[reader] failed to start thread\n");
        serial_close(&r->serial);
        return false;
    }
    return true;
}

void reader_join(Reader* r) {
    if (r->thread) {
        WaitForSingleObject(r->thread, INFINITE);
        CloseHandle(r->thread);
        r->thread = NULL;
    }
    serial_close(&r->serial);
}
