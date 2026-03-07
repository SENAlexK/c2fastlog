#include <iostream>
#include <thread>
#include <vector>

#include <c2_fastlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

int main() {
    // Create a colored console sink
    auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    console_sink->set_level(spdlog::level::trace);
    console_sink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [thread %t] %v");

    // Create C2SpdLogger with the sink
    auto logger = std::make_shared<c2fastlog::detail::C2SpdLogger>("c2fastlog", console_sink);
    logger->set_level(spdlog::level::trace);

    // Initialize c2fastlog
    c2fastlog::initialize(logger, c2fastlog::LogLevel::trace, std::chrono::seconds(1));

    // Basic logging examples
    C2_LOG_TRACE("This is a trace message");
    C2_LOG_DEBUG("This is a debug message");
    C2_LOG_INFO("This is an info message");
    C2_LOG_WARN("This is a warning message");
    C2_LOG_ERROR("This is an error message");

    // Logging with format arguments
    C2_LOG_INFO("Integer: {}, Float: {:.2f}, String: {}", 42, 3.14159, "hello");

    // Multi-threaded logging example
    std::vector<std::thread> threads;
    for (int i = 0; i < 4; ++i) {
        threads.emplace_back([i]() {
            for (int j = 0; j < 10; ++j) {
                C2_LOG_INFO("Thread {} - Message {}", i, j);
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    // Wait for logs to be flushed
    std::this_thread::sleep_for(std::chrono::seconds(2));

    // Release resources
    c2fastlog::release();

    std::cout << "Example completed successfully!" << std::endl;
    return 0;
}
