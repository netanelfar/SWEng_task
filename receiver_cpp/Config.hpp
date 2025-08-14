#pragma once
#include <cstddef> 
// Networking
constexpr unsigned short LISTENER_PORT = 5555;

// Pool
constexpr std::size_t POOL_PREALLOC_NODES = 1024;

// Writer
constexpr std::size_t WRITER_FLUSH_EVERY = 100;
constexpr std::size_t WRITER_STDIO_BUFFER_KB = 1024;
constexpr const char* WRITER_OUTPUT_FILE = "packets.bin";
