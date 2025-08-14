#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <windows.h>

typedef struct {
    uint8_t*         buf;      // storage
    size_t           cap;      // capacity in bytes
    size_t           head;     // write index (0..cap-1)
    size_t           tail;     // read index  (0..cap-1)
    size_t           size;     // bytes currently stored
    CRITICAL_SECTION cs;       // protects fields above
    CONDITION_VARIABLE can_read;   // signaled when size increases
    CONDITION_VARIABLE can_write;  // signaled when free space increases
    bool             closed;   // true -> wake waiters and stop
} ByteRing;

bool rb_init(ByteRing* rb, size_t capacity);
void rb_free(ByteRing* rb);
void rb_close(ByteRing* rb);

// Blocking push: copies ALL 'len' bytes (waits if full).
void rb_push_bytes(ByteRing* rb, const uint8_t* src, size_t len);

// Blocking pop: waits until at least 'len' bytes are available,
// then copies them out (FIFO). For us: len = 100.
void rb_pop_exact(ByteRing* rb, uint8_t* dst, size_t len);
