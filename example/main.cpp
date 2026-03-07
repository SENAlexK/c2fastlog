#include "Logger.h"

#include <chrono>
#include <thread>

int main()
{
    // 初始化日志
    x2trader::setupLog( "info", "c2log_example.log", "c2log" );

    // 基本日志示例
    LOG_INFO( "Hello, c2log!" );
    LOG_DEBUG( "This is a debug message" );
    LOG_WARN( "This is a warning" );
    LOG_ERROR( "This is an error" );

    // 格式化日志示例
    int orderId = 12345;
    double price = 100.5;
    int volume = 1000;
    LOG_INFO( "Order: id={}, price={:.2f}, volume={}", orderId, price, volume );

    // 循环日志示例
    for( int i = 0; i < 10; ++i )
    {
        LOG_INFO( "Loop iteration: {}", i );
    }

    // 多线程日志示例
    auto worker = []( int threadNum ) {
        for( int i = 0; i < 5; ++i )
        {
            LOG_INFO( "Thread {} - message {}", threadNum, i );
            std::this_thread::sleep_for( std::chrono::milliseconds( 10 ) );
        }
    };

    std::thread t1( worker, 1 );
    std::thread t2( worker, 2 );

    t1.join();
    t2.join();

    LOG_INFO( "All threads completed" );

    // 等待日志写入完成
    std::this_thread::sleep_for( std::chrono::milliseconds( 100 ) );

    return 0;
}
