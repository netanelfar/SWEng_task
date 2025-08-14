#include "serial.h"
#include <stdio.h>
#include <string.h>
#include <windows.h>

static bool build_com_path(const char *name, char outPath[64])
{
    if (!name)
        return false;
    if (_strnicmp(name, "\\\\.\\", 4) == 0)
    {
        strncpy(outPath, name, 63);
        outPath[63] = 0;
        return true;
    }
    _snprintf_s(outPath, 64, _TRUNCATE, "\\\\.\\%s", name);
    return true;
}

static DWORD clamp_baud(DWORD req)
{
    // Default if not provided
    if (req == 0)
        req = 28800;
    // Enforce minimum link rate of 28,800 bps
    if (req < 28800)
        req = 28800;
    return req;
}

bool serial_open(SerialCtx *sc, const ReaderConfig *cfg)
{
    memset(sc, 0, sizeof *sc);
    sc->cfg = *cfg;
    sc->h = INVALID_HANDLE_VALUE;

    char path[64];
    if (!build_com_path(cfg->com_name, path))
        return false;

    HANDLE h = CreateFileA(path, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
    if (h == INVALID_HANDLE_VALUE)
    {
        fprintf(stderr, "[serial] CreateFile(%s) failed (%lu)\n", path, GetLastError());
        return false;
    }

    // Timeouts: frequent wakeups, low latency reads
    COMMTIMEOUTS to = {0};
    to.ReadIntervalTimeout = 1;       // ms between bytes
    to.ReadTotalTimeoutConstant = 10; // total cap
    to.ReadTotalTimeoutMultiplier = 0;
    to.WriteTotalTimeoutConstant = 10;
    to.WriteTotalTimeoutMultiplier = 0;
    if (!SetCommTimeouts(h, &to))
    {
        fprintf(stderr, "[serial] SetCommTimeouts failed (%lu)\n", GetLastError());
        CloseHandle(h);
        return false;
    }

    DCB dcb = {0};
    dcb.DCBlength = sizeof dcb;
    if (!GetCommState(h, &dcb))
    {
        fprintf(stderr, "[serial] GetCommState failed (%lu)\n", GetLastError());
        CloseHandle(h);
        return false;
    }

    DWORD reqBaud = clamp_baud(cfg->baud);
    dcb.BaudRate = reqBaud; // request at least 28800 (or higher as given)
    dcb.ByteSize = 8;
    dcb.Parity = NOPARITY; // 8-N-1
    dcb.StopBits = ONESTOPBIT;
    dcb.fOutxCtsFlow = FALSE;
    dcb.fOutxDsrFlow = FALSE;
    dcb.fDtrControl = DTR_CONTROL_DISABLE;
    dcb.fRtsControl = RTS_CONTROL_DISABLE;
    dcb.fOutX = FALSE;
    dcb.fInX = FALSE;

    if (!SetCommState(h, &dcb))
    {
        fprintf(stderr, "[serial] SetCommState failed (%lu)\n", GetLastError());
        CloseHandle(h);
        return false;
    }

    // Read back to see what the driver actually accepted
    if (!GetCommState(h, &dcb))
    {
        fprintf(stderr, "[serial] GetCommState (verify) failed (%lu)\n", GetLastError());
        CloseHandle(h);
        return false;
    }

    SetupComm(h, 64 * 1024, 16 * 1024);
    PurgeComm(h, PURGE_RXCLEAR | PURGE_TXCLEAR);

    sc->h = h;

    // Approximate max bytes/sec for 8-N-1: baud/10
    double estimated_Bps = (double)dcb.BaudRate / 10.0;
    if (estimated_Bps < 2000.0)
    {
        // This is just a warning; the device may still produce bursts but average < spec.
        fprintf(stderr, "[serial] WARNING: effective baud %lu -> ~%.0f B/s (< 2000 B/s target)\n",
                (unsigned long)dcb.BaudRate, estimated_Bps);
    }

    printf("[serial] open %s, requested %lu, effective %lu (~%.0f B/s)\n",
           path, (unsigned long)reqBaud, (unsigned long)dcb.BaudRate, estimated_Bps);

    return true;
}

size_t serial_read_some(SerialCtx *sc, uint8_t *dst, size_t cap)
{
    DWORD nread = 0;
    BOOL ok = ReadFile(sc->h, dst, (DWORD)cap, &nread, NULL);
    if (!ok)
    {
        DWORD e = GetLastError();
        if (e == ERROR_DEVICE_NOT_CONNECTED ||
            e == ERROR_INVALID_HANDLE ||
            e == ERROR_ACCESS_DENIED ||
            e == ERROR_OPERATION_ABORTED)
        {
            fprintf(stderr, "[serial] device disconnected (err=%lu)\n", e);
            return SIZE_MAX; // notify caller to shutdown
        }
        // transient error -> soft retry
        Sleep(1);
        return 0;
    }
    if (nread == 0)
    {
        // timeout with no data: small nap
        Sleep(1);
    }
    return (size_t)nread;
}

void serial_close(SerialCtx *sc)
{
    if (sc->h != INVALID_HANDLE_VALUE)
    {
        CloseHandle(sc->h);
        sc->h = INVALID_HANDLE_VALUE;
        printf("[serial] closed\n");
    }
}
