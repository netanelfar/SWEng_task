# SWEng_task

## introduction 

This project implements the required two applications:

- **Receiver (C++)** — Listens on a TCP socket, re-frames the incoming byte stream into fixed **100-byte** packets, appends them to a **binary file**, and prints a short summary to console.
- **Sender (C)** — Reads bytes from either a **real Windows COM port** or an **emulator**, groups them into **100-byte** packets, and sends them over TCP to the receiver.

The assignment spec is intentionally loose, so I aimed to keep the solution simple, robust, and easy to reason about—while still demonstrating fundamentals a junior engineer should know: TCP framing, serial I/O on Windows, threading, buffering/backpressure, and clean build scripts.

Limitations & Future Work:

- **Security:** Plain TCP. Future: TLS (OpenSSL/SChannel) for encryption + auth.
- **Integrity:** No magic word / sequence / CRC. Future: add a tiny header `{magic, seq32, crc32}` (still 100B payload is possible if you wrap just for monitoring, or bump to 104–112B if allowed).
- **Multi-client:** Receiver accepts one client. Future: accept loop + per-client listener feeding a shared writer (with source ID).
- **Backpressure policy:** Pool currently grows on demand (no drops). Future: optional bounded mode (block producer or drop oldest), expose metrics.
- **Formal tests:** Add unit tests for framing and pool behavior; add an integration test harness that replays captured serial data.

 

you will need the following to build the apps:

* windows OS

* Desktop development with C++

* **MSVC v14.4x** toolset (latest)

* **CMake** ≥ **3.26** 

  

for real COM testing:

* Know the **COM port number**  (Device Manager → “Ports (COM & LPT)”) and update run_com.bat with the right port.





## Requirements Interpretation

- **Packet size:** Exactly **100 bytes** per packet on the wire.
- **Rate:** At least **20 packets/sec** ⇒ **2,000 bytes/sec** (≈16 kbps).
- **Source:** Data originates from a **COM port** (real or emulated).
- **Transport:** Sender → Receiver over **TCP**.
- **Receiver duties:** Persist to binary file and print to console.

**Implications:**
 TCP is a **byte stream**, so the receiver must **re-frame** the stream into 100-byte units using a reliable `recvAll(100)` loop. The COM source may be bursty; buffering and backpressure help avoid loss.

## Receiver (C++) Architecture

![alt text](.\receiver.png)

### **Threads & data path**

- `ListenerThread` → `DoubleListPool` (ready queue) → `WriterThread`
- `WriterThread` writes `100B` exactly per packet; flushes periodically (configurable) and prints a short “first 4 bytes” line every *N* packets.

### **Synchronization**

- One mutex guarding the lists, two condition variables to sleep when empty/full.
- `pool.close()` signals shutdown; writer drains and exits cleanly.

### **Configurables (via `Config.hpp`)**

- `LISTENER_PORT` (default 5555)
- `POOL_PREALLOC_NODES` (e.g., 1024)
- `WRITER_FLUSH_EVERY` (e.g., 100)
- `WRITER_STDIO_BUFFER_KB` (e.g., 1024)
- `WRITER_OUTPUT_FILE` (default `packets.bin`)
- `PRINT_EVERY` (e.g., 20 for COM so you see output regularly)

## Sender (C) Architecture

![alt text](.\sender.png)



### **Backends**

- Emulator: token-bucket pacing to approximate baud/10 bytes/sec (8-N-1), with a floor of 2,000 B/s to guarantee ≥20 pkt/s.
- Windows COM: `CreateFileA` on `\\.\COMx`, `SetCommState` to requested `baud` (clamped to ≥28,800), non-blocking timeouts tuned for frequent reads.

### **Threads & data path**

- Reader thread → byte ring/buffer → Packer thread → `send_all(100B)` over TCP.

### **CLI**

- COM: `--com COMx`, `--baud`

## Buffering & Concurrency Design

We use **two different structures** for two different problems:

1. **Ring Buffer (SPSC)** in the **sender (C)**
    *Problem:* move an unbounded **byte stream** from the serial reader to the packer with minimal latency and zero lock contention.
2. **DoubleListPool (free list + ready FIFO)** in the **receiver (C++)**
    *Problem:* pass **fixed 100-byte packets** from the listener to the writer, avoid heap churn, provide clean backpressure & shutdown.

They’re intentionally different because the workloads are different: *bytes vs packets* and *hot path lock-free vs simple blocking hand-off*.

### Sender Ring Buffer (Single-Producer / Single-Consumer)

A **byte-oriented** circular buffer used between the **serial Reader thread** (producer) and the **Packer thread** (consumer).

Why this choice:

- The sender’s **reader** and **packer** are exactly one producer and one consumer → SPSC fits perfectly.
- We need to move **bytes** (not 100B units) because the serial device may deliver any chunk size at any time.
- Lock-free hot path: producer and consumer update different atomics → no mutex on the steady state, just memory ordering.
- Win32 **CRITICAL_SECTION + CONDITION_VARIABLE** are **fast** in-process primitives (lighter than a kernel mutex/event pair).
- Blocking semantics keep the code **simple and lossless** at the assignment’s rates (≥2 KB/s), while still handling bursts.

Uses a **CRITICAL_SECTION** to guard metadata (`head`, `tail`, `size`) and two **CONDITION_VARIABLES** to **block** without spinning:

- `can_write` — producer waits here when the buffer is *full*.
- `can_read` — consumer waits here when the buffer is *empty* (or has fewer than `len` bytes for `pop_exact`).

#### API

* **`rb_push_bytes` is \*all-or-block\***: it copies **all** `len` bytes, waiting as long as needed for space. This gives **lossless backpressure** if the consumer falls behind.

* **`rb_pop_exact` waits for \*len\* bytes**: consumer blocks until **at least `len` bytes** are available, then copies exactly `len` in FIFO order. This is ideal for the packer, which needs **exact 100B** frames regardless of serial chunking.

* **`rb_close`**: sets `closed = true` and **wakes both** wait sets; waiters re-check state, finish what’s possible, and return (your threads then exit cleanly).



### Receiver DoubleListPool (Free List + Ready FIFO)

What it is

An **intrusive object pool** of `Node` objects, each holding **exactly 100 bytes**. We maintain two linked lists:

- **Free list** — nodes available to be filled
- **Ready list** — nodes filled by the listener, waiting for the writer

Intrusive = the `next` pointer lives **inside** the node; no separate queue nodes → fewer allocations, better cache locality.

Why this choice

- The receiver naturally works in **packet units** (100B) — nodes are a perfect fit.
- We want to **recycle** memory (avoid malloc/free per packet) and have a clear place to implement **backpressure** or **growth**.
- Simpler than lock-free MPMC queues; we only have one producer and one consumer, and we value clarity + correctness.

#### API

- `Node* getFree()`
   Pop from free list. If empty, **allocate** a new node (current policy = no drops).
- `bool addNode(Node*)`
   Push to **tail** of ready FIFO and `notify_one()` the writer.
- `Node* getNode()`
   **Blocking** pop from ready FIFO; waits on `cv_not_empty`. Returns `nullptr` when pool is **closed and drained**.
- `void addFree(Node*)`
   Return the node to the free list (push front).
- `void close()`
   Mark closed and notify both CVs to let threads exit cleanly.

All public methods take the internal mutex as needed; simple to reason about. The internal helpers are “unsafe” only in the sense of “must be called with the mutex held.”





### How they fit together end-to-end

- **Sender:** Serial → **Ring (bytes)** → Packer → TCP
   *Rationale:* arbitrary byte chunks, hot path, SPSC lock-free.
- **Receiver:** TCP → **Pool (100B nodes)** → File/Console
   *Rationale:* natural packet unit, cheap reuse, easy backpressure and clean shutdown.

### Risks, Trade-offs, and Mitigations

- **Risk:** Writer stalls → ready queue grows (memory).
   **Mitigation:** stdio buffering + periodic flush; optional bounded mode if required.
- **Risk:** Console printing is slow.
   **Mitigation:** print every *N* packets and **flush** (`std::endl` or `std::cerr`); tune `PRINT_EVERY` via config.
- **Risk:** Serial bursts.
   **Mitigation:** sender’s ring absorbs the burst; receiver’s socket `SO_RCVBUF` increased (e.g., 512 KB).
- **Risk:** Partial TCP reads.
   **Mitigation:** `recvAll(100)` reframes exactly 100B every time.

## Build & Run

**Two build folders, two modes:**

- `build_emulator.bat` → `build-emul\sender.exe` + `receiver.exe`
- `build_com.bat` → `build-com\sender.exe` + `receiver.exe`

after building :

`run_emulator.bat` if you do not have Arduino available

if you have an Arduino you can use:

```c
void setup() 
{
 Serial.begin(115200);
}

byte b = 65;
void loop() 
{
  Serial.print('$'); // Packet start char
  for (int i = 0; i < 98; ++i)
    Serial.print((char)b);
  Serial.print('#'); // Packet end char
  delay(1000);  
  ++b;
  if (b > 90) // 90 - 'Z'
    b = 65;   // 65 - 'A'
}
```

and after you update the `run_com.bat` you can run it.
file + console example for emulator run:
![alt text](.\cEmu.png)
![alt text](.\fileEmu.png)
file example for Arduino run:
![alt text](.\eArduino.jpg)
