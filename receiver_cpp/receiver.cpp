#include "Config.hpp"
#include "DoubleListPool.hpp"
#include "ListenerThread.hpp"
#include "WriterThread.hpp"

int main() {
    DoubleListPool pool(POOL_PREALLOC_NODES);
    ListenerThread listener(LISTENER_PORT, pool);
    WriterThread writer(pool, WRITER_OUTPUT_FILE);

    writer.setFlushEvery(WRITER_FLUSH_EVERY);
    writer.setStdioBufferKB(WRITER_STDIO_BUFFER_KB);

    if (!listener.start()) return 1;
    if (!writer.start()) { listener.stop(); return 1; }
    writer.wait();  
    writer.stop();
    listener.stop();
}
