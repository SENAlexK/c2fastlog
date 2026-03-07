#include "Logger.h"

#include <chrono>
#include <iomanip>
#include <iostream>

int main()
{
    constexpr size_t LOG_COUNT = 100'000'000;  // 一亿条

    // 初始化日志
    x2trader::setupLog( "info", "bench_c2fastlog.log", "bench" );

    std::cout << "=============================================================================\n";
    std::cout << "c2fastlog Benchmark\n";
    std::cout << "Total logs: " << LOG_COUNT / 1'000'000 << " million\n";
    std::cout << "Log message: \"Order: id=<n>, price=12345.6789, volume=100\"\n";
    std::cout << "=============================================================================\n\n";

    std::cout << "Warming up...\n";
    for( size_t i = 0; i < 10000; ++i )
    {
        LOG_INFO( "Order: id={}, price={:.4f}, volume={}", i, 12345.6789, 100 );
    }

    std::cout << "Running benchmark...\n";

    auto start = std::chrono::high_resolution_clock::now();

    for( size_t i = 0; i < LOG_COUNT; ++i )
    {
        LOG_INFO( "Order: id={}, price={:.4f}, volume={}", i, 12345.6789, 100 );
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>( end - start ).count();

    double seconds = duration / 1000.0;
    double throughput = LOG_COUNT / seconds;
    double latency = ( duration * 1'000'000.0 ) / LOG_COUNT;  // ns per log

    std::cout << "\nResults:\n";
    std::cout << std::fixed << std::setprecision( 2 );
    std::cout << "  Time:       " << seconds << " seconds\n";
    std::cout << "  Throughput: " << std::setprecision( 2 ) << throughput / 1'000'000 << " M logs/sec\n";
    std::cout << "  Latency:    " << std::setprecision( 2 ) << latency << " ns/log\n";
    std::cout << "\nBenchmark completed.\n";

    // 等待日志写入完成
    std::this_thread::sleep_for( std::chrono::seconds( 5 ) );

    return 0;
}
