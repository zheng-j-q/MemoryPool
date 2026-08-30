#ifndef MEMORY_POOL_H
#define MEMORY_POOL_H

#include <cstddef>
#include <atomic>
#include <cstdlib>

/**
 * 16字节对齐的内存块节点（用于空闲链表）
 * alignas(16) 保证整个结构体按16字节对齐，从而确保后续有效载荷也天然对齐
 */
struct alignas(16) BlockNode {
    BlockNode* next;  // 指向下一个空闲块的指针
};

/**
 * 固定大小、无锁、内存对齐的高并发内存池
 * 线程安全：使用 std::atomic 实现无锁栈，无全局互斥锁，适合高并发场景
 */
class MemoryPool {
public:
    /**
     * @param blockSize   每个内存块的有效载荷大小（用户实际可用的字节数）
     * @param blockCount  内存池中总共包含的块数量（注意：不是总字节数）
     *
     * 示例：MemoryPool pool(sizeof(int), 100000);
     *       表示创建10万个大小为4字节（实际对齐后约24字节）的块
     */
    MemoryPool(size_t blockSize, size_t blockCount);
    ~MemoryPool();

    // 禁止拷贝和赋值（资源独占）
    MemoryPool(const MemoryPool&) = delete;
    MemoryPool& operator=(const MemoryPool&) = delete;

    // 分配一个内存块，返回指向有效载荷的指针（自动16字节对齐）
    void* allocate();

    // 释放内存块，将其归还到空闲链表
    void deallocate(void* ptr);

    // ----- 统计信息（便于测试和监控）-----
    size_t getBlockSize()    const { return m_blockSize; }
    size_t getTotalBlocks()  const { return m_totalBlocks; }
    size_t getUsedBlocks()   const { return m_usedBlocks.load(std::memory_order_relaxed); }
    size_t getAvailableBlocks() const { return m_freeBlocks.load(std::memory_order_relaxed); }

private:
    // 计算实际每个块占用的总大小（包含 BlockNode 头部，且16字节对齐）
    size_t calculateAlignedBlockSize() const;

    char* m_memoryPool = nullptr;           // 原始大块内存起始地址
    size_t m_blockSize = 0;                 // 用户有效载荷大小（例如 4 字节）
    size_t m_alignedBlockSize = 0;          // 实际每个块占用的总大小（例如 24 字节）
    size_t m_totalBlocks = 0;               // 总块数

    std::atomic<BlockNode*> m_freeList{ nullptr };  // 无锁空闲链表栈顶
    std::atomic<size_t> m_usedBlocks{ 0 };          // 已分配块计数（统计用）
    std::atomic<size_t> m_freeBlocks{ 0 };          // 空闲块计数（统计用）
};

#endif // MEMORY_POOL_H