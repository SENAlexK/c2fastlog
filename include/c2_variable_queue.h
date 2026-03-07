#pragma once

#include "utils.h"

#include <cstdint>
#include <cstring>

namespace c2fastlog {

// ============================================================================
// Variable Message Structure
// ============================================================================

struct VariableMessage {
    std::uint32_t type = 0;
    std::uint32_t size = 0;  // size == 0 indicates padding block, next block jumps to index 0
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#endif
    std::uint8_t content[];  // Flexible array member (C99 extension, widely supported)
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif
};

// ============================================================================
// SPSC Variable Queue
// ============================================================================

// Thread-safe variable-length message SPSC queue.
// SIZE must be greater than twice the maximum message length.
template <std::uint32_t SIZE>
class SPSCVariableQueue {
    static_assert(SIZE >= 1024, "SIZE must be at least 1024 bytes");

public:
    static constexpr std::uint32_t msg_header_size = sizeof(VariableMessage);
    static constexpr std::uint32_t epsilon = msg_header_size - 1;
    static constexpr std::uint32_t capacity = SIZE / msg_header_size;

    SPSCVariableQueue() = default;
    ~SPSCVariableQueue() = default;

    // Non-copyable and non-movable
    SPSCVariableQueue(const SPSCVariableQueue&) = delete;
    SPSCVariableQueue& operator=(const SPSCVariableQueue&) = delete;
    SPSCVariableQueue(SPSCVariableQueue&&) = delete;
    SPSCVariableQueue& operator=(SPSCVariableQueue&&) = delete;

    // Allocate space for a new message with content_size bytes.
    // Returns pointer to message on success, nullptr on failure.
    // After success, calling allocate() or write_commit() before write_commit() invalidates the pointer.
    [[nodiscard]] inline VariableMessage* allocate(std::uint32_t content_size) noexcept {
        const auto alloc_block = (msg_header_size + content_size + epsilon) / msg_header_size;

        if (alloc_block >= free_block_) [[unlikely]] {
            const auto tail_cache = tail_;
            if (head_ >= tail_cache) {
                // Writer and reader are in the same cycle, writer is ahead of reader.
                // data_: |_|_|_|_|_|_|_|_|_|
                //            tail---^  head---^
                free_block_ = capacity - head_;
                if (alloc_block >= free_block_ && tail_cache != 0) {
                    // Not enough space at end, wrap around to beginning.
                    // Mark current position as padding block.
                    data_[head_].type = 0;
                    data_[head_].size = 0;
                    head_ = 0;
                    free_block_ = tail_cache;
                }
            } else {
                // Writer has wrapped, reader is ahead of writer.
                // data_: |_|_|_|_|_|_|_|_|_|
                //            head---^  tail---^
                free_block_ = tail_cache - head_;
            }
            if (alloc_block >= free_block_) [[unlikely]] {
                return nullptr;
            }
        }

        data_[head_].type = 0;
        data_[head_].size = content_size;
        return &data_[head_];
    }

    // Commit the write operation.
    inline void write_commit() noexcept {
        const auto alloc_block = (msg_header_size + data_[head_].size + epsilon) / msg_header_size;
        if (free_block_ > capacity) {
            free_block_ = capacity;
        }
        free_block_ -= alloc_block;
        head_ += alloc_block;
    }

    // Get the front message for reading.
    // Returns nullptr if queue is empty.
    [[nodiscard]] inline const VariableMessage* front() noexcept {
        const auto head_cache = head_;
        if (tail_ != head_cache && data_[tail_].size == 0) {
            tail_ = 0;
        }
        if (tail_ == head_cache) [[unlikely]] {
            return nullptr;
        }
        return &data_[tail_];
    }

    // Commit the read operation.
    inline void read_commit() noexcept {
        if (tail_ == head_) [[unlikely]] {
            return;
        }
        tail_ += (msg_header_size + data_[tail_].size + epsilon) / msg_header_size;
        // Check for padding block
        if (tail_ != head_ && data_[tail_].size == 0) {
            tail_ = 0;
        }
    }

    // Check if the queue is empty.
    [[nodiscard]] inline bool empty() const noexcept {
        return tail_ == head_;
    }

    // Allocate with discard-oldest policy.
    // When buffer is full, discard oldest messages until there's enough space.
    // WARNING: This modifies tail_, not suitable for multi-threaded SPSC scenarios!
    // Only use in single-threaded scenarios or when consumer thread is stopped.
    // Returns: allocated pointer, or nullptr if single message exceeds queue capacity.
    [[nodiscard]] inline VariableMessage* allocate_discard_oldest(std::uint32_t content_size) noexcept {
        const auto alloc_block = (msg_header_size + content_size + epsilon) / msg_header_size;

        // Check if single message exceeds maximum capacity
        if (alloc_block >= capacity) [[unlikely]] {
            return nullptr;
        }

        // Try normal allocation
        auto ptr = allocate(content_size);
        if (ptr != nullptr) [[likely]] {
            return ptr;
        }

        // Allocation failed, need to discard oldest messages
        while (ptr == nullptr) {
            if (tail_ == head_) {
                // Queue is empty, reset pointers
                head_ = 0;
                tail_ = 0;
                free_block_ = capacity;
                break;
            }
            discard_oldest_one();
            ptr = allocate(content_size);
        }

        if (ptr == nullptr) {
            ptr = allocate(content_size);
        }
        return ptr;
    }

private:
    // Discard the oldest message (internal use only)
    inline void discard_oldest_one() noexcept {
        if (tail_ == head_) {
            return;
        }

        // Check for padding block
        if (data_[tail_].size == 0) {
            tail_ = 0;
            return;
        }

        // Calculate blocks occupied by current message
        const auto tail_block = (msg_header_size + data_[tail_].size + epsilon) / msg_header_size;
        tail_ += tail_block;

        // Check for padding block after move
        if (tail_ != head_ && data_[tail_].size == 0) {
            tail_ = 0;
        }
    }

private:
    volatile std::size_t head_ = 0;      // Write pointer
    std::size_t free_block_ = capacity;  // Free block count

    alignas(cache_line_size) volatile std::size_t tail_ = 0;  // Read pointer
    alignas(cache_line_size) VariableMessage data_[capacity];
};

}  // namespace c2fastlog
