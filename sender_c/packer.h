#pragma once
#include <windows.h>
#include <winsock2.h>
#include <stdint.h>
#include "ring_buffer.h"
#include "tcp.h"

#define FRAME_SIZE 100

// Thread function: pops 100B frames from ring and sends them over 'sock'.
unsigned __stdcall packer_thread(void* sock_and_rb);

// Helper to pack args for the thread
typedef struct {
    SOCKET    sock;
    ByteRing* rb;
} PackerArgs;
