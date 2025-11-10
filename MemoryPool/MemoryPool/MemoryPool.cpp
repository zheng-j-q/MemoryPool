#include "MemoryPool.h"
#include <stdexcept>  //提供异常类型：std::bad_malloc,std::invalid_argument

// 内存对齐计算 
//定义常量：内存对齐字节 后续将m_blocksize向上对齐到16的倍数 提升CPU的访问效率
constexpr size_t MEM_ALIGNMENT = 16;


//构造函数：分配内存池并初始化空闲链表
//blocksize向上对齐到MEM_ALIGNMENT的倍数：(目标+对齐-1)&~(对齐-1) 对齐必须是2的整数次幂
//初始化空闲链表头指针为空，空闲链表暂未创建
MemoryPool::MemoryPool(size_t blockSize, size_t blockCount)
    : m_blockSize((blockSize + MEM_ALIGNMENT - 1) & ~(MEM_ALIGNMENT - 1)),m_blockCount(blockCount),
    m_head(nullptr) ,m_blocks(blockCount){

    if (m_blockSize == 0 || blockCount == 0) {
        throw std::invalid_argument("Block size or count cannot be zero"); //块大小或数量为0，抛出异常
    }

    // 预分配大块连续内存
    char* pool = new char[m_blockSize * blockCount]; //分配总内存=对齐后块大小*块数量
    m_pools.push_back(pool);  //将分配的内存地址存入m_pools，管理所有预分配的内存

    // 初始化链表
    //MemoryBlock** current = &m_head; //辅助指针，指向当前节点的指针地址
    for (size_t i = 0; i < blockCount; ++i) //遍历每个块，创建MemoryBlock节点
        {  //为第i个块创建节点
            m_blocks[i].data = pool + i * m_blockSize;  //data：当前块的地址-pool基地址+偏移量
            m_blocks[i].isUsed = false;                   //isUsed：初始为空闲，未使用
            m_blocks[i].next = m_head;                  //next：下一个节点暂时为空
            m_head = &m_blocks[i];          ///更新链表头指针，指向当前节点
        }
}

//析构函数：释放所有内存块节点和预分配的内存池
MemoryPool::~MemoryPool() {
    // 释放内存池
    for (auto pool : m_pools) {
        delete[] pool;
    }
}

//内存分配函数：从空闲链表中取出一个块，标记为已使用并返回其地址
void* MemoryPool::allocate() {
    //加锁：确保多线程安全，会阻塞其他线程直到当前线程释放锁
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!m_head) {  //如果空闲链表为空，表示没有可用块，抛出std::bad_alloc（标准内存分配失败异常）
        throw std::bad_alloc();  // 无可用内存块
    }

    MemoryBlock* block = m_head;  //取出链表头结点
    m_head = m_head->next;     //空闲链表头指针后移
    block->isUsed = true;      //标记当前块为已使用
    return block->data;       //返回当前块的实际内存地址
}

//内存释放函数：将ptr指向的块释放回内存池，标记空闲并重新加入空闲链表
void MemoryPool::deallocate(void* ptr) {
    if (!ptr) return;  //避免空指针操作

    std::lock_guard<std::mutex> lock(m_mutex); //加锁：确保多线程安全

    // 通过内存偏移找到对应的MemoryBlock
    char* blockStart = reinterpret_cast<char*>(ptr); //将ptr转换为char* 按字节计算地址
    for (auto pool : m_pools) { //遍历所有预分配的内存池
        //检查ptr是否在当前pool范围内，如果在：
        if (blockStart >= pool && blockStart < pool + m_blockSize * m_blockCount) {
            size_t blockIndex = (blockStart - pool) / m_blockSize;
            if (blockIndex >= m_blockCount) {
                throw std::invalid_argument("Invalid pointer: out of pool range");
            }
            MemoryBlock* block = &m_blocks[blockIndex];
            if (block->isUsed == false) {
                throw std::invalid_argument("Double free detected"); // 避免重复释放
            }
            //ptr对应的节点地址=pool基地址+块索引*节点大小  块索引=（块偏移量/块大小）
            block->isUsed = false; //标记块为空闲
            block->next = m_head;  //将块插入链表头部
            m_head = block;    //更新链表头指针
            return;
        }
    }
    throw std::invalid_argument("Pointer not from this pool");  //ptr不属于任何内存池，抛出异常
}

//诊断信息函数：计算当前内存池中剩余的空闲块数量
size_t MemoryPool::getAvailableBlocks() const {
    size_t count = 0;  //计数器
    for (const auto& block : m_blocks) {
        if (!block.isUsed) {
            ++count;
        }
    }
    return count;
}
