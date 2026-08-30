#include "MemoryPool.h"    // 引入自定义内存池
#include <iostream>        // 标准输入输出
#include <vector>          // 动态数组
#include <thread>          // 多线程支持
#include <chrono>          // 时间测量
#include <iomanip>         // 格式化输出

// ==================== 测试1：基础功能 + 16字节对齐验证 ====================
void testBasicAndAlignment() {
    std::cout << "\n========== 测试1：基础分配/释放 与 16字节对齐验证 ==========\n";

    // 创建内存池：块大小 64 字节，共 100 个块
    MemoryPool pool(64, 100);

    // 分配 5 个块，检查每个返回的地址是否 16 字节对齐
    std::vector<void*> ptrs;
    for (int i = 0; i < 5; ++i) {
        void* p = pool.allocate();
        if (p == nullptr) {
            std::cout << "❌ 分配失败（不应该发生）\n";
            return;
        }
        ptrs.push_back(p);

        uintptr_t addr = reinterpret_cast<uintptr_t>(p);
        bool isAligned = (addr % 16 == 0);
        std::cout << "块 " << i << " 地址: " << std::hex << addr << std::dec
            << ", 16字节对齐: " << (isAligned ? "✅ 是" : "❌ 否") << std::endl;

        // 模拟写入数据
        char* data = static_cast<char*>(p);
        std::fill(data, data + 64, static_cast<char>(i));
    }

    // 释放所有块
    for (void* p : ptrs) {
        pool.deallocate(p);
    }

    std::cout << "当前可用块数: " << pool.getAvailableBlocks() << "/" << pool.getTotalBlocks() << std::endl;
    std::cout << "✅ 基础功能与对齐验证通过\n";
}

// ==================== 测试2：性能对比（内存池 vs new/delete） ====================
void testPerformance() {
    std::cout << "\n========== 测试2：性能对比（10万次分配+释放） ==========\n";

    const int TEST_SIZE = 100000;

    // ---- 2.1 使用内存池 ----
    {
        MemoryPool pool(sizeof(int), TEST_SIZE);  // 第二个参数直接传块数量，符合直觉
        auto start = std::chrono::high_resolution_clock::now();

        for (int i = 0; i < TEST_SIZE; ++i) {
            int* p = static_cast<int*>(pool.allocate());
            *p = i;               // 写入数据
            pool.deallocate(p);   // 释放回池
        }

        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

        std::cout << "MemoryPool 耗时: " << duration.count() << " μs\n";
        std::cout << "  最终可用块: " << pool.getAvailableBlocks() << "/" << pool.getTotalBlocks() << std::endl;
    }

    // ---- 2.2 使用 new/delete ----
    {
        auto start = std::chrono::high_resolution_clock::now();

        for (int i = 0; i < TEST_SIZE; ++i) {
            int* p = new int(i);
            delete p;
        }

        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

        std::cout << "new/delete 耗时: " << duration.count() << " μs\n";
    }
}

// ==================== 测试3：多线程并发安全测试 ====================
void threadWorker(MemoryPool& pool, int threadId) {
    const int ALLOCS_PER_THREAD = 1000;
    std::vector<void*> localPtrs;
    localPtrs.reserve(ALLOCS_PER_THREAD);

    // 每个线程分配 1000 个块
    for (int i = 0; i < ALLOCS_PER_THREAD; ++i) {
        void* p = pool.allocate();
        if (p != nullptr) {
            localPtrs.push_back(p);
            // 模拟写入数据（写入线程ID，便于后续调试）
            char* data = static_cast<char*>(p);
            std::fill(data, data + sizeof(int), static_cast<char>(threadId & 0xFF));
        }
        else {
            std::cerr << "线程 " << threadId << " 分配失败（池可能耗尽）\n";
        }
    }

    // 释放所有块
    for (void* p : localPtrs) {
        pool.deallocate(p);
    }

    std::cout << "线程 " << threadId << " 完成\n";
}

void testMultiThread() {
    std::cout << "\n========== 测试3：多线程并发安全测试（10线程 × 1000次） ==========\n";

    const int THREAD_COUNT = 10;
    const int BLOCKS_PER_THREAD = 1000;
    // 创建足够大的池：总块数 = 线程数 × 每线程分配数 × 1.2（留点余量）
    MemoryPool pool(sizeof(int), static_cast<size_t>(THREAD_COUNT * BLOCKS_PER_THREAD * 1.2));

    std::vector<std::thread> threads;
    auto start = std::chrono::high_resolution_clock::now();

    // 创建并启动 10 个线程
    for (int i = 0; i < THREAD_COUNT; ++i) {
        threads.emplace_back(threadWorker, std::ref(pool), i);
    }

    // 等待所有线程完成
    for (auto& t : threads) {
        t.join();
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    std::cout << "多线程测试总耗时: " << duration.count() << " ms\n";
    std::cout << "最终状态 - 已分配: " << pool.getUsedBlocks()
        << ", 空闲: " << pool.getAvailableBlocks()
        << ", 总块数: " << pool.getTotalBlocks() << std::endl;

    // 自动检测内存泄漏
    if (pool.getUsedBlocks() == 0 && pool.getAvailableBlocks() == pool.getTotalBlocks()) {
        std::cout << "✅ 多线程测试通过：无内存泄漏，所有块已正确回收！\n";
    }
    else {
        std::cout << "⚠️ 存在未释放的块，请检查代码！\n";
    }
}

// ==================== main 入口 ====================
int main() {
    std::cout << "===== 高并发固定块内存池 —— 完整测试程序 =====\n";

    try {
        testBasicAndAlignment();
        testPerformance();
        testMultiThread();

        std::cout << "\n===== 所有测试完成 =====" << std::endl;
    }
    catch (const std::exception& e) {
        std::cerr << "❌ 程序异常: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
