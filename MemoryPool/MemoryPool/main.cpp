#include "MemoryPool.h"  //引入自定义内存池声明
#include <iostream> //标准输入输出
#include <vector>  //动态数组
#include <thread>  //多线程支持
#include <chrono>  //时间测量

// 性能对比测试
//对比内存池 allocate/deallocate 与原生 new/delete 的性能差异
void testPerformance() {
    //测试次数 10万次内存分配+释放
    const int TEST_SIZE = 100000;

    // 使用内存池
    {
        MemoryPool pool(sizeof(int), TEST_SIZE); //创建内存池，块大小为int4字节，块数量10万
        auto start = std::chrono::high_resolution_clock::now(); //记录开始时间

        for (int i = 0; i < TEST_SIZE; ++i) {
            int* p = static_cast<int*>(pool.allocate());  //从内存池分配一个int块
            *p = i;               //使用分配的内存
            pool.deallocate(p);   //释放内存回内存池
        }

        auto end = std::chrono::high_resolution_clock::now();  //记录结束时间

        std::cout << "MemoryPool time: "
            << std::chrono::duration_cast<std::chrono::microseconds>(end - start).count()
            << " μs\n";  //输出内存池分配释放总时间，单位为微妙
    } //内存池对象超出作用域，自动析构释放内存

    // 使用new/delete
    {
        auto start = std::chrono::high_resolution_clock::now();  //记录开始时间

        for (int i = 0; i < TEST_SIZE; ++i) {
            int* p = new int(i);
            delete p;
        }

        auto end = std::chrono::high_resolution_clock::now();

        std::cout << "new/delete time: "
            << std::chrono::duration_cast<std::chrono::microseconds>(end - start).count()
            << " μs\n";
    }
}

// 多线程安全测试
//多个线程同时从同一个内存池分配和释放内存，验证内存池在多线程并发调用时的安全性
void threadTest(MemoryPool& pool, int id) { //内存池引用，线程ID
    std::vector<void*> ptrs;  //存储分配的内存块指针
    //每个线程分配1000个内存块
    for (int i = 0; i < 1000; ++i) {
        ptrs.push_back(pool.allocate()); //从共享内存池分配块，存入ptrs
    }
    //释放所有内存块
    for (auto p : ptrs) {
        pool.deallocate(p);
    }
    //输出线程完成信息
    std::cout << "Thread " << id << " completed\n";
}

int main() {
    // 基础测试
    //验证内存池的基本分配和释放功能
    MemoryPool pool(sizeof(double), 5); //创建内存池，块大小为double8字节，块数量为5
    double* d1 = static_cast<double*>(pool.allocate());  //分配一个double块
    double* d2 = static_cast<double*>(pool.allocate());  //在分配一个double块
    *d1 = 3.14; //写入数据 使用分配的内存
    *d2 = 2.718; 
    std::cout << "Available blocks: " << pool.getAvailableBlocks() << "/5\n"; //输出剩余可用块3/5

    pool.deallocate(d1); //释放块
    pool.deallocate(d2);
    std::cout << "Available blocks: " << pool.getAvailableBlocks() << "/5\n\n";  //输出剩余可用块5/5

    // 性能测试
    testPerformance();

    // 多线程测试
    MemoryPool threadPool(sizeof(int), 10000); //创建共享内存池，块大小为int4字节，块数量为1万
    std::vector<std::thread> threads; //存储线程对象
    //创建10个线程 并发调用threadTest函数
    for (int i = 0; i < 10; ++i) {
        //emplace_back直接构造线程对象，避免拷贝；参数为线程函数、内存池引用、线程ID
        threads.emplace_back(threadTest, std::ref(threadPool), i);
    }
    //等待所有线程完成
    for (auto& t : threads) {
        t.join(); //主线程阻塞，等待子线程完成
    }
    //输出多线程测试完成信息
    std::cout << "\nThread-safe test completed!\n";

    return 0;
}
