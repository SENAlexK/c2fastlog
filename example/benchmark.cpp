#include <chrono>
#include <iomanip>
#include <iostream>

#include <c2_fastlog.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/null_sink.h>

constexpr std::size_t warmup_count = 100'000;
constexpr std::size_t benchmark_count = 10'000'000;

int main() {
    // Create a null sink for pure throughput measurement
    auto null_sink = std::make_shared<spdlog::sinks::null_sink_mt>();
    auto logger = std::make_shared<c2fastlog::detail::C2SpdLogger>("bench", null_sink);
    logger->set_level(spdlog::level::trace);

    // Initialize c2fastlog
    c2fastlog::initialize(logger, c2fastlog::LogLevel::trace, std::chrono::seconds(60));

    std::cout << "=== C2FastLog Benchmark ===" << std::endl;
    std::cout << std::endl;

    // Warm-up phase
    std::cout << "Warming up with " << warmup_count << " logs..." << std::endl;
    for (std::size_t i = 0; i < warmup_count; ++i) {
        C2_LOG_INFO("Warmup: id={}, price={:.4f}, volume={}", i, 12345.6789, 100);
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // Benchmark phase
    std::cout << "Running benchmark with " << benchmark_count << " logs..." << std::endl;

    auto start = std::chrono::high_resolution_clock::now();

    for (std::size_t i = 0; i < benchmark_count; ++i) {
        C2_LOG_INFO("Order: id={}, price={:.4f}, volume={}", i, 12345.6789, 100);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);

    // Calculate metrics
    double total_seconds = duration.count() / 1e9;
    double logs_per_second = benchmark_count / total_seconds;
    double ns_per_log = static_cast<double>(duration.count()) / benchmark_count;

    std::cout << std::endl;
    std::cout << "=== Results ===" << std::endl;
    std::cout << std::fixed << std::setprecision(2);
    std::cout << "Total logs:      " << benchmark_count << std::endl;
    std::cout << "Total time:      " << total_seconds << " s" << std::endl;
    std::cout << "Throughput:      " << logs_per_second / 1e6 << " M logs/sec" << std::endl;
    std::cout << "Latency:         " << ns_per_log << " ns/log" << std::endl;

    // Wait for all logs to be processed
    std::cout << std::endl;
    std::cout << "Waiting for logs to flush..." << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(5));

    // Release resources
    c2fastlog::release();

    std::cout << "Benchmark completed!" << std::endl;
    return 0;
}
