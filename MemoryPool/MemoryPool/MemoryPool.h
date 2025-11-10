#pragma once
#include <vector>  //用于管理预分配的内存池 m_pools
#include <mutex>   //用于线程安全锁  m_mutex
#include <cstddef>  //提供size_t, ptrdiff_t


//MemoryPool是一个线程安全的固定大小内存池实现，
// 核心功能是通过预分配内存块、管理空闲链表，实现高效的内存分配与释放，
// 适用于频繁申请、释放同大小内存的的场景
//比如 对象池、链表节点分配等

class MemoryPool {  //定义一个内存池类，用于高效分配和释放固定大小的内存块
public:
    // 构造函数 (块大小, 块数量)
    //在构造时一次性分配 blockSize * blockCount 字节的内存，并将这些内存分割为多个独立的块，用链表管理空闲块
    MemoryPool(size_t blockSize, size_t blockCount);

    // 禁用拷贝构造
    MemoryPool(const MemoryPool&) = delete;
    //禁用拷贝赋值
    MemoryPool& operator=(const MemoryPool&) = delete;

    ~MemoryPool();

    // 分配内存块函数
    //从内存池中分配一个空闲块，并返回其地址
    void* allocate();

    // 释放内存块函数
    //将之前分配的内存块（ptr 指向的地址）释放回内存池，标记为“空闲”并重新加入空闲链表
    void deallocate(void* ptr);

    // 诊断信息函数
    //返回当前内存池中剩余的空闲块数量（用于调试或监控内存池使用情况）
    size_t getAvailableBlocks() const;

private:
    //定义内存块的元数据结构，用于跟踪每个块的地址、使用状态和链表关系
    struct MemoryBlock {
        void* data;         // 内存块起始地址，指向实际可分配的内存空间
        bool isUsed;        // 使用状态：true已使用、false空闲
        MemoryBlock* next;  // 链表指针，指向下一个空闲块
    };

    const size_t m_blockSize;     // 每个内存块固定大小，构造时确定，不可修改
    const size_t m_blockCount;    //内存块总数量，构造时确定，不可修改
    MemoryBlock* m_head;          // 空闲链表头指针，指向第一个空闲块，分配时从此取块
    std::vector<char*> m_pools;   // 预分配的原始内存地址-内存池
    std::vector<MemoryBlock> m_blocks; //内存块元数据数组 ，跟踪每个块的使用状态和链表关系
    std::mutex m_mutex;           // 线程安全锁-互斥锁，确保多线程下allocate/deallocate的原子性操作
};
