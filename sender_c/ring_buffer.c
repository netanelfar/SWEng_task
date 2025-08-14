#include "ring_buffer.h"
#include <stdlib.h>
#include <string.h>

static __forceinline size_t minz(size_t a, size_t b){ return a < b ? a : b; }

bool rb_init(ByteRing* rb, size_t capacity){
    if(!rb || capacity==0) return false;
    rb->buf = (uint8_t*)malloc(capacity);
    if(!rb->buf) return false;
    rb->cap = capacity;
    rb->head = rb->tail = rb->size = 0;
    rb->closed = false;
    InitializeCriticalSection(&rb->cs);
    InitializeConditionVariable(&rb->can_read);
    InitializeConditionVariable(&rb->can_write);
    return true;
}

void rb_free(ByteRing* rb){
    if(!rb) return;
    EnterCriticalSection(&rb->cs);
    uint8_t* p = rb->buf;
    rb->buf=NULL; rb->cap=rb->head=rb->tail=rb->size=0;
    LeaveCriticalSection(&rb->cs);
    if(p) free(p);
    DeleteCriticalSection(&rb->cs);
    // CONDITION_VARIABLE has no destroy API on Windows
}

void rb_close(ByteRing* rb){
    EnterCriticalSection(&rb->cs);
    rb->closed = true;
    WakeAllConditionVariable(&rb->can_read);
    WakeAllConditionVariable(&rb->can_write);
    LeaveCriticalSection(&rb->cs);
}

void rb_push_bytes(ByteRing* rb, const uint8_t* src, size_t len){
    size_t off = 0;
    while(off < len){
        EnterCriticalSection(&rb->cs);
        while(!rb->closed && rb->size == rb->cap){
            SleepConditionVariableCS(&rb->can_write, &rb->cs, INFINITE);
        }
        if(rb->closed){ LeaveCriticalSection(&rb->cs); return; }

        size_t free_space = rb->cap - rb->size;
        size_t chunk = minz(len - off, free_space);

        // write up to end
        size_t right = rb->cap - rb->head;
        size_t first = minz(chunk, right);
        memcpy(rb->buf + rb->head, src + off, first);
        rb->head = (rb->head + first) % rb->cap;
        rb->size += first;
        off += first;

        // wrap to start if needed
        size_t left = chunk - first;
        if(left){
            memcpy(rb->buf + rb->head, src + off, left);
            rb->head = (rb->head + left) % rb->cap;
            rb->size += left;
            off += left;
        }

        WakeConditionVariable(&rb->can_read);
        LeaveCriticalSection(&rb->cs);
    }
}

void rb_pop_exact(ByteRing* rb, uint8_t* dst, size_t len){
    size_t out = 0;
    while(out < len){
        EnterCriticalSection(&rb->cs);
        while(!rb->closed && rb->size == 0){
            SleepConditionVariableCS(&rb->can_read, &rb->cs, INFINITE);
        }
        if(rb->closed && rb->size == 0){ LeaveCriticalSection(&rb->cs); return; }

        size_t avail = rb->size;
        size_t chunk = minz(len - out, avail);

        size_t right = rb->cap - rb->tail;
        size_t first = minz(chunk, right);
        memcpy(dst + out, rb->buf + rb->tail, first);
        rb->tail = (rb->tail + first) % rb->cap;
        rb->size -= first;
        out += first;

        size_t left = chunk - first;
        if(left){
            memcpy(dst + out, rb->buf + rb->tail, left);
            rb->tail = (rb->tail + left) % rb->cap;
            rb->size -= left;
            out += left;
        }

        WakeConditionVariable(&rb->can_write);
        LeaveCriticalSection(&rb->cs);
    }
}
