#pragma once
#include <windows.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

typedef struct {
    bool   use_serial;      // false = emulator
    char   com_name[64];    // e.g. "COM3" or "\\\\.\\COM12"
    DWORD  baud;            // e.g. 115200
} ReaderConfig;

typedef struct {
    ReaderConfig cfg;
    HANDLE       h;         // serial handle (INVALID_HANDLE_VALUE if emulator)
} SerialCtx;

// Init serial (or emulator). Returns true on success.
bool serial_open(SerialCtx* sc, const ReaderConfig* cfg);

// Read up to 'cap' bytes into 'dst'. Blocks briefly; returns bytes read (0..cap).
// In emulator mode this just generates bytes.
size_t serial_read_some(SerialCtx* sc, uint8_t* dst, size_t cap);

// Close & cleanup.
void serial_close(SerialCtx* sc);
