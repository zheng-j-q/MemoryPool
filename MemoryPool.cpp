#include "MemoryPool.h"
#include <iostream>
#include <stdexcept>
#include <new>   // std::bad_alloc

// 工具函数：将 size 向上对齐到 16 的整数倍
static inline size_t alignUpTo16(size_t size) {
    return (size + 15) & ~15;
}

// ----- 构造函数：根据 块大小 和 块数量 初始化内存池 -----
MemoryPool::MemoryPool(size_t blockSize, size_t blockCount)
    : m_blockSize(blockSize)
{
    if (blockSize == 0 || blockCount == 0) {
        throw std::invalid_argument("blockSize 和 blockCount 必须大于 0");
    }

    // 1. 计算每个块实际占用的总大小（BlockNode头部 + 用户数据，并做16字节对齐）
    size_t rawBlockSize = sizeof(BlockNode) + blockSize;
    m_alignedBlockSize = alignUpTo16(rawBlockSize);

    // 2. 根据块数量计算总内存大小（不再由用户传字节数，避免歧义）
    m_totalBlocks = blockCount;
    size_t totalMemoryBytes = m_alignedBlockSize * blockCount;

    // 3. 分配大块原始内存
    m_memoryPool = static_cast<char*>(std::malloc(totalMemoryBytes));
    if (m_memoryPool == nullptr) {
        throw std::bad_alloc();
    }

    // 4. 初始化无锁空闲链表：将所有块通过 BlockNode::next 串联起来（头插法）
    BlockNode* head = nullptr;
    for (size_t i = 0; i < m_totalBlocks; ++i) {
        char* blockStart = m_memoryPool + i * m_alignedBlockSize;
        BlockNode* node = reinterpret_cast<BlockNode*>(blockStart);
        node->next = head;   // 新节点指向当前栈顶
        head = node;         // 更新栈顶
    }
    m_freeList.store(head, std::memory_order_release);

    // 5. 初始化统计信息
    m_freeBlocks.store(m_totalBlocks, std::memory_order_relaxed);
    m_usedBlocks.store(0, std::memory_order_relaxed);

    std::cout << "[MemoryPool] 初始化成功: 总块数=" << m_totalBlocks
        << ", 有效载荷=" << m_blockSize << " 字节"
        << ", 实际每块=" << m_alignedBlockSize << " 字节"
        << ", 总内存=" << totalMemoryBytes / (1024.0 * 1024.0) << " MB"
        << std::endl;
}

// ----- 析构函数：释放原始大块内存 -----
MemoryPool::~MemoryPool() {
    if (m_memoryPool != nullptr) {
        std::free(m_memoryPool);
        m_memoryPool = nullptr;
    }
}

// ----- 分配内存（无锁栈 Pop 操作）-----
void* MemoryPool::allocate() {
    BlockNode* oldHead = m_freeList.load(std::memory_order_acquire);

    while (oldHead != nullptr) {
        BlockNode* newHead = oldHead->next;
        // 尝试原子地将栈顶从 oldHead 更新为 newHead
        if (m_freeList.compare_exchange_weak(oldHead, newHead,
            std::memory_order_release,
            std::memory_order_relaxed)) {
            // 分配成功，更新统计
            m_freeBlocks.fetch_sub(1, std::memory_order_relaxed);
            m_usedBlocks.fetch_add(1, std::memory_order_relaxed);

            // 返回有效载荷指针（跳过 BlockNode 头部）
            char* blockStart = reinterpret_cast<char*>(oldHead);
            return static_cast<void*>(blockStart + sizeof(BlockNode));
        }
        // 如果 CAS 失败，oldHead 被自动更新为最新栈顶，继续循环重试
    }

    // 内存池耗尽，返回 nullptr
    return nullptr;
}

// ----- 释放内存（无锁栈 Push 操作）-----
void MemoryPool::deallocate(void* ptr) {
    if (ptr == nullptr) return;

    // 计算 BlockNode 的起始地址（有效载荷地址 - 头部大小）
    char* payloadStart = static_cast<char*>(ptr);
    char* blockStart = payloadStart - sizeof(BlockNode);
    BlockNode* node = reinterpret_cast<BlockNode*>(blockStart);

    // 无锁栈 Push：将当前节点插入栈顶
    BlockNode* oldHead = m_freeList.load(std::memory_order_acquire);
    do {
        node->next = oldHead;
    } while (!m_freeList.compare_exchange_weak(oldHead, node,
        std::memory_order_release,
        std::memory_order_relaxed));

    // 更新统计
    m_freeBlocks.fetch_add(1, std::memory_order_relaxed);
    m_usedBlocks.fetch_sub(1, std::memory_order_relaxed);
}