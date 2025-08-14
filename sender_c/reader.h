#pragma once
#include <windows.h>
#include <stdbool.h>
#include "serial.h"
#include "ring_buffer.h"

// Opaque-ish reader that owns the serial ctx + thread
typedef struct {
    HANDLE      thread;         // _beginthreadex handle
    SerialCtx   serial;         // COM handle or emulator
    ByteRing*   rb;             // where to push bytes
    volatile LONG* running;     // shared stop flag
} Reader;

// Start the reader thread. If cfg->use_serial == false, uses emulator.
// Returns false on failure (e.g., cannot open COM).
bool reader_start(Reader* r, const ReaderConfig* cfg, ByteRing* rb, volatile LONG* running);

// Join/close. Safe to call even if reader_start failed partially.
void reader_join(Reader* r);
