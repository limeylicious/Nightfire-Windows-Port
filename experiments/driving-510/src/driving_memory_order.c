/* Host-side ordering for guest code and software device models sharing RAM.
 * No emulated dirty-cache layer or physical Xbox DMA exists in this runtime.
 * This orders host memory operations; it does NOT complete GPU commands or
 * reproduce physical WBINVD cache invalidation. Device flushes still pass
 * through the existing MMIO model and its completion polling.
 */
#ifdef _WIN32
#include <windows.h>
#else
#include <stdatomic.h>
#endif
void nightfire_memory_barrier(void)
{
#ifdef _WIN32
    MemoryBarrier();
#else
    atomic_thread_fence(memory_order_seq_cst);
#endif
}
