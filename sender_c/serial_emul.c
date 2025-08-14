#include "serial.h"
#include <windows.h>
#include <string.h>
#include <stdio.h>

// Simple token-bucket pacing at a target byte rate = max(2000 B/s, baud/10)
// (8-N-1 framing -> ~10 bits/byte). Default baud if unset: 28800 bps.

static uint8_t s_byte_counter = 0;

// High-resolution state
static LARGE_INTEGER s_freq;
static LARGE_INTEGER s_last_tick;
static double        s_bytes_per_sec = 0.0;
static double        s_budget_bytes  = 0.0;
static BOOL          s_inited        = FALSE;

static void pace_init_if_needed(DWORD baud)
{
    if (s_inited) return;

    // bytes/sec ~= baud / 10 (8N1). Default baud if 0: 28800.
    double bps = (baud ? (double)baud : 28800.0) / 10.0;
    if (bps < 2000.0) bps = 2000.0;   // enforce spec minimum (20 * 100B)

    QueryPerformanceFrequency(&s_freq);
    QueryPerformanceCounter(&s_last_tick);
    s_bytes_per_sec = bps;
    s_budget_bytes  = 0.0;
    s_inited        = TRUE;

    printf("[emul] target ~%.0f B/s (baud=%lu)\n", s_bytes_per_sec, (unsigned long)(baud ? baud : 28800));
}

bool serial_open(SerialCtx* sc, const ReaderConfig* cfg)
{
    memset(sc, 0, sizeof *sc);
    sc->h = INVALID_HANDLE_VALUE; // emulator has no real handle
    sc->cfg = *cfg;               // keep baud if provided
    pace_init_if_needed(cfg ? cfg->baud : 0);
    return true;
}

size_t serial_read_some(SerialCtx* sc, uint8_t* dst, size_t cap)
{
    (void)sc;

    // Update token bucket based on elapsed time
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    double elapsed = (double)(now.QuadPart - s_last_tick.QuadPart) / (double)s_freq.QuadPart;
    s_last_tick = now;

    s_budget_bytes += elapsed * s_bytes_per_sec;
    if (s_budget_bytes < 1.0) {
        // Not enough budget to emit even 1 byte yet — sleep a tiny bit
        Sleep(1);
        return 0; // caller will loop again
    }

    // How many bytes can we emit this call?
    // Cap the burst size to avoid giant writes if we paused (e.g., 4096).
    size_t max_burst = 4096;
    size_t want = (size_t)s_budget_bytes;
    if (want > max_burst) want = max_burst;
    if (want > cap)       want = cap;

    // Generate 'want' bytes with a simple pattern
    for (size_t i = 0; i < want; ++i) {
        dst[i] = s_byte_counter++;
    }

    s_budget_bytes -= (double)want;
    return want;
}

void serial_close(SerialCtx* sc)
{
    (void)sc; // nothing to do for emulator
}
