#pragma once

#include "c2_packer.h"
#include "c2_spdlog.h"
#include "c2_variable_queue.h"

#include <algorithm>
#include <atomic>
#include <limits>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

namespace c2fastlog {
namespace detail {

// ============================================================================
// Configuration
// ============================================================================

#ifndef FASTLOG_THREAD_BUFFER_SIZE
#define FASTLOG_THREAD_BUFFER_SIZE (1 << 24)  // 16 MB
#endif

// ============================================================================
// Thread Buffer
// ============================================================================

class ThreadBufferDestroyer;

// Thread buffer for log data
class ThreadBuffer {
public:
    static constexpr std::size_t buffer_size = FASTLOG_THREAD_BUFFER_SIZE;
    using BufferType = SPSCVariableQueue<buffer_size>;

    ThreadBuffer() = default;
    ~ThreadBuffer() = default;

    // Non-copyable
    ThreadBuffer(const ThreadBuffer&) = delete;
    ThreadBuffer& operator=(const ThreadBuffer&) = delete;

    [[nodiscard]] BufferType& get_buffer() noexcept {
        return buffer_;
    }

    void set_thread_id(std::size_t thread_id) noexcept {
        thread_id_ = thread_id;
    }

    [[nodiscard]] std::size_t get_thread_id() const noexcept {
        return thread_id_;
    }

    [[nodiscard]] bool should_deallocate() const noexcept {
        return should_deallocate_;
    }

private:
    friend class ThreadBufferDestroyer;

    void release() noexcept {
        should_deallocate_ = true;
    }

private:
    BufferType buffer_;
    std::size_t thread_id_ = 0;
    bool should_deallocate_ = false;
};

// Thread-local buffer instance
inline thread_local std::shared_ptr<ThreadBuffer> t_thread_buffer;

// RAII wrapper for thread buffer cleanup
class ThreadBufferDestroyer {
public:
    ThreadBufferDestroyer() = default;

    ~ThreadBufferDestroyer() {
        if (t_thread_buffer != nullptr) {
            t_thread_buffer->release();
            t_thread_buffer = nullptr;
        }
    }

    // Non-copyable
    ThreadBufferDestroyer(const ThreadBufferDestroyer&) = delete;
    ThreadBufferDestroyer& operator=(const ThreadBufferDestroyer&) = delete;
};

inline thread_local ThreadBufferDestroyer t_thread_buffer_destroyer;

// ============================================================================
// Static Log Info
// ============================================================================

// Static information for each unique log call site
struct StaticLogInfo {
    using FormatFunction = std::string (*)(fmt::string_view, const char*);

    spdlog::source_loc source;
    spdlog::level::level_enum level;
    fmt::string_view fmt;
    FormatFunction fmt_fn;

    StaticLogInfo(spdlog::source_loc src, LogLevel lvl, fmt::string_view format, FormatFunction fn)
        : source(src), level(turn_level_to_spdlog(lvl)), fmt(format), fmt_fn(fn) {
    }
};

// ============================================================================
// Fast Logger
// ============================================================================

class FastLogger {
    FastLogger() = default;

    ~FastLogger() {
        release();
    }

public:
    // Singleton access
    [[nodiscard]] static FastLogger& instance() {
        static FastLogger logger;
        return logger;
    }

    // Non-copyable and non-movable
    FastLogger(const FastLogger&) = delete;
    FastLogger& operator=(const FastLogger&) = delete;
    FastLogger(FastLogger&&) = delete;
    FastLogger& operator=(FastLogger&&) = delete;

    // Initialize the logger
    void init(std::shared_ptr<C2SpdLogger> logger,
              LogLevel logging_level = LogLevel::trace,
              std::chrono::seconds flush_interval = std::chrono::seconds(5)) {
        if (logger == nullptr) {
            return;
        }

        spdlogger_ = std::move(logger);
        logging_level_ = logging_level;
        flush_interval_ = flush_interval;

        // Create logging thread
        std::unique_lock<std::mutex> lock(logging_thread_mutex_);
        if (logging_thread_.joinable()) {
            running_ = false;
            logging_thread_.join();
        }
        running_ = true;
        logging_thread_ = std::thread(&FastLogger::logging_thread_func, &FastLogger::instance());
    }

    // Release resources and flush pending logs
    void release() {
        if (!running_) {
            return;
        }
        check_buffer_again_ = true;
        running_ = false;

        std::unique_lock<std::mutex> thread_lock(logging_thread_mutex_);
        if (logging_thread_.joinable()) {
            logging_thread_.join();
        }

        std::unique_lock<std::mutex> buffers_lock(thread_buffers_mutex_);
        free_thread_buffers_.clear();
    }

    // Logging level accessors
    void set_logging_level(LogLevel level) noexcept {
        logging_level_ = level;
    }

    [[nodiscard]] LogLevel get_logging_level() const noexcept {
        return logging_level_;
    }

    // Allocate thread buffer
    void allocate(std::size_t thread_id) {
        if (t_thread_buffer != nullptr) {
            return;
        }

        std::unique_lock<std::mutex> lock(thread_buffers_mutex_);
        if (!free_thread_buffers_.empty()) {
            t_thread_buffer = free_thread_buffers_.back();
            free_thread_buffers_.pop_back();
        } else {
            t_thread_buffer = std::make_shared<ThreadBuffer>();
        }
        t_thread_buffer->set_thread_id(thread_id);
        thread_buffers_.push_back(t_thread_buffer);
    }

    // Get thread buffer
    [[nodiscard]] auto& get_thread_buffer() noexcept {
        return t_thread_buffer->get_buffer();
    }

    // Report buffer size exhaustion
    void report_buffer_exhausted() noexcept {
        buffer_size_enough_ = false;
    }

    // Register static log info
    void register_log_info(std::uint32_t& log_id, StaticLogInfo info) {
        if (log_id != 0) {
            return;
        }
        std::unique_lock<std::mutex> lock(static_log_infos_mutex_);
        log_id = static_cast<std::uint32_t>(static_log_infos_.size() + 1);
        static_log_infos_.push_back(std::move(info));
    }

private:
    // Logging thread main function
    void logging_thread_func() {
        // Comparator for min-heap based on first message timestamp
        auto msg_timestamp_compare = [](const std::shared_ptr<ThreadBuffer>& lhs, const std::shared_ptr<ThreadBuffer>& rhs) {
            Timestamp l_timestamp = std::numeric_limits<Timestamp>::max();
            Timestamp r_timestamp = std::numeric_limits<Timestamp>::max();

            if (auto* l_msg = lhs->get_buffer().front(); l_msg != nullptr) {
                l_timestamp = *reinterpret_cast<const Timestamp*>(l_msg->content);
            }
            if (auto* r_msg = rhs->get_buffer().front(); r_msg != nullptr) {
                r_timestamp = *reinterpret_cast<const Timestamp*>(r_msg->content);
            }
            return l_timestamp > r_timestamp;
        };

        auto log_infos = std::vector<StaticLogInfo>();
        auto logger = spdlogger_;
        auto last_flush_time = spdlog::details::os::now();

        while (running_ || check_buffer_again_) {
            check_buffer_again_ = false;

            // Ensure free buffer pool is adequate
            if (free_thread_buffers_.size() < min_free_buffer_count) {
                std::unique_lock<std::mutex> lock(thread_buffers_mutex_);
                while (free_thread_buffers_.size() < max_free_buffer_count) {
                    free_thread_buffers_.push_back(std::make_shared<ThreadBuffer>());
                }
            }

            // Update static log info cache
            if (static_log_infos_.size() > log_infos.size()) {
                std::unique_lock<std::mutex> lock(static_log_infos_mutex_);
                for (auto i = log_infos.size(); i < static_log_infos_.size(); ++i) {
                    log_infos.push_back(static_log_infos_[i]);
                }
            }

            // Process logs
            std::unique_lock<std::mutex> lock(thread_buffers_mutex_);

            // Release thread buffers for terminated threads
            for (std::size_t i = 0; i < thread_buffers_.size();) {
                auto& tb = thread_buffers_[i];
                if (tb->get_buffer().front() == nullptr && tb->should_deallocate()) {
                    thread_buffers_[i] = thread_buffers_.back();
                    thread_buffers_.pop_back();
                } else {
                    ++i;
                }
            }

            const auto start_consume = get_timestamp_ns();

            // Build min-heap based on first message timestamp
            std::make_heap(thread_buffers_.begin(), thread_buffers_.end(), msg_timestamp_compare);

            while (!thread_buffers_.empty() && thread_buffers_.front()->get_buffer().front() != nullptr) {
                auto& tb = thread_buffers_.front();
                auto& buffer = tb->get_buffer();
                const auto* msg = buffer.front();
                const auto* content = reinterpret_cast<const char*>(msg->content);
                const auto& timestamp = *reinterpret_cast<const Timestamp*>(content);

                if (timestamp > start_consume || msg->type > log_infos.size()) {
                    check_buffer_again_ = true;
                    break;
                }

                const auto& log_info = log_infos[msg->type - 1];
                logger->log(tb->get_thread_id(),
                            timestamp,
                            log_info.source,
                            log_info.level,
                            log_info.fmt_fn(log_info.fmt, content + sizeof(Timestamp)));
                buffer.read_commit();

                // Maintain heap structure after processing
                std::pop_heap(thread_buffers_.begin(), thread_buffers_.end(), msg_timestamp_compare);
                std::push_heap(thread_buffers_.begin(), thread_buffers_.end(), msg_timestamp_compare);
            }
            lock.unlock();

            // Report buffer exhaustion warning
            if (!buffer_size_enough_) {
                static std::once_flag flag;
                std::call_once(flag, [&]() {
                    logger->log(spdlog::details::os::thread_id(),
                                get_timestamp_ns(),
                                spdlog::source_loc{__FILE__, __LINE__, __func__},
                                spdlog::level::err,
                                "FastLog thread buffer size not enough!");
                });
            }

            // Periodic flush
            if (spdlog::details::os::now() - last_flush_time > flush_interval_) {
                logger->flush();
                last_flush_time = spdlog::details::os::now();
            }

            std::this_thread::sleep_for(std::chrono::microseconds(1));
        }
    }

private:
    // Logging thread state
    volatile bool running_ = false;
    volatile bool check_buffer_again_ = false;
    mutable std::mutex logging_thread_mutex_;
    std::thread logging_thread_;
    std::shared_ptr<C2SpdLogger> spdlogger_;
    LogLevel logging_level_ = LogLevel::trace;
    std::chrono::seconds flush_interval_;

    // Thread buffer management
    mutable std::mutex thread_buffers_mutex_;
    std::vector<std::shared_ptr<ThreadBuffer>> thread_buffers_;
    std::vector<std::shared_ptr<ThreadBuffer>> free_thread_buffers_;
    bool buffer_size_enough_ = true;
    static constexpr std::size_t min_free_buffer_count = 8;
    static constexpr std::size_t max_free_buffer_count = 32;

    // Static log info registry
    std::mutex static_log_infos_mutex_;
    std::vector<StaticLogInfo> static_log_infos_;
};

}  // namespace detail
}  // namespace c2fastlog
