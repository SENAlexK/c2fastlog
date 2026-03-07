<p align="center">
  <h1 align="center">C2FastLog</h1>
  <p align="center">
    <b>面向高频交易系统的超低延迟异步日志库</b>
  </p>
  <p align="center">
    <a href="README.md">English</a> | <a href="README_CN.md"><b>中文</b></a>
  </p>
</p>

<p align="center">
  <img src="https://img.shields.io/badge/license-MIT-blue.svg" alt="License">
  <img src="https://img.shields.io/badge/C%2B%2B-20-blue.svg" alt="C++20">
  <img src="https://img.shields.io/badge/platform-Linux%20%7C%20Windows-lightgrey.svg" alt="Platform">
  <img src="https://img.shields.io/badge/header--only-yes-green.svg" alt="Header Only">
</p>

---

## 概述

**C2FastLog** 是一个高性能、纯头文件的异步日志库，专为高频交易（HFT）等对延迟敏感的应用场景设计。通过将格式化操作推迟到后台线程并使用无锁 SPSC 队列，实现了 **亚200纳秒** 的日志延迟。

### 核心特性

- **超低延迟**：热路径每条日志约 ~170ns
- **高吞吐量**：每秒 500 万+ 条日志
- **热路径零分配**：预分配的线程本地缓冲区
- **无锁设计**：SPSC 队列最小化竞争
- **纯头文件**：简单集成，即包即用
- **现代 C++20**：充分利用最新语言特性
- **spdlog 后端**：灵活的输出目标（控制台、文件等）
- **线程安全**：每个线程独立缓冲区，无竞争

## 性能指标

| 指标 | 数值 |
|------|------|
| 吞吐量 | **5.7+ M 日志/秒** |
| 平均延迟 | **~174 纳秒/日志** |
| 缓冲区大小 | 每线程 16 MB（可配置）|

*测试环境：Linux + GCC 15，Release 构建（-O3）*

## 架构设计

```
┌─────────────────────────────────────────────────────────────────┐
│                          生产者线程                               │
├─────────────┬─────────────┬─────────────┬─────────────┬─────────┤
│   线程 1    │   线程 2    │   线程 3    │    ...      │  线程 N │
│ ┌─────────┐ │ ┌─────────┐ │ ┌─────────┐ │             │         │
│ │  SPSC   │ │ │  SPSC   │ │ │  SPSC   │ │             │         │
│ │   队列  │ │ │   队列  │ │ │   队列  │ │             │         │
│ └────┬────┘ │ └────┬────┘ │ └────┬────┘ │             │         │
└──────┼──────┴──────┼──────┴──────┼──────┴─────────────┴─────────┘
       │             │             │
       └─────────────┼─────────────┘
                     │
                     ▼
       ┌─────────────────────────────┐
       │         后台线程            │
       │  ┌───────────────────────┐  │
       │  │     最小堆合并        │  │
       │  │   (按时间戳排序)      │  │
       │  └───────────┬───────────┘  │
       │              │              │
       │  ┌───────────▼───────────┐  │
       │  │     格式化 & 输出     │  │
       │  │   (spdlog 后端)       │  │
       │  └───────────────────────┘  │
       └─────────────────────────────┘
```

### 设计理念

1. **热路径优化**：仅捕获原始参数，延迟格式化
2. **线程隔离**：无锁 SPSC 队列消除竞争
3. **时间戳排序**：后台线程按时间戳合并日志
4. **灵活后端**：利用 spdlog 丰富的 sink 生态

## 快速开始

### 环境要求

- 支持 C++20 的编译器（GCC 10+、Clang 12+、MSVC 2019+）
- CMake 3.20+
- spdlog（作为子模块包含）

### 安装步骤

```bash
# 克隆仓库（含子模块）
git clone --recursive https://github.com/anthropics/c2fastlog.git
cd c2fastlog

# 构建
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)

# 运行示例
./c2fastlog_example
./c2fastlog_benchmark
```

### 基本用法

```cpp
#include <c2_fastlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

int main() {
    // 1. 创建 sink（兼容 spdlog）
    auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    console_sink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [线程 %t] %v");

    // 2. 创建 C2FastLog 日志器
    auto logger = std::make_shared<c2fastlog::detail::C2SpdLogger>("app", console_sink);
    logger->set_level(spdlog::level::trace);

    // 3. 初始化
    c2fastlog::initialize(logger, c2fastlog::LogLevel::trace, std::chrono::seconds(1));

    // 4. 使用宏记录日志
    C2_LOG_TRACE("这是一条 trace 消息");
    C2_LOG_DEBUG("调试值: {}", 42);
    C2_LOG_INFO("用户 {} 从 {} 登录", "alice", "192.168.1.1");
    C2_LOG_WARN("内存使用率 {}%", 85);
    C2_LOG_ERROR("连接失败: {}", "超时");

    // 5. 清理
    std::this_thread::sleep_for(std::chrono::seconds(1));
    c2fastlog::release();
    return 0;
}
```

### CMake 集成

```cmake
# 方式 1：作为子目录添加
add_subdirectory(third_party/c2fastlog)
target_link_libraries(your_target PRIVATE c2fastlog::c2fastlog)

# 方式 2：find_package（安装后）
find_package(c2fastlog REQUIRED)
target_link_libraries(your_target PRIVATE c2fastlog::c2fastlog)
```

## API 参考

### 初始化函数

```cpp
namespace c2fastlog {

// 初始化日志系统
void initialize(
    std::shared_ptr<detail::C2SpdLogger> logger,  // 基于 spdlog 的日志器
    LogLevel logging_level,                        // 最低日志级别
    std::chrono::seconds flush_interval            // 自动刷新间隔
);

// 预分配线程缓冲区（可选，首次日志时自动分配）
void preallocate();

// 释放资源并刷新所有待处理日志
void release();

}
```

### 日志宏

| 宏 | 说明 |
|----|------|
| `C2_LOG_TRACE(fmt, ...)` | Trace 级别日志（最详细）|
| `C2_LOG_DEBUG(fmt, ...)` | Debug 级别日志 |
| `C2_LOG_INFO(fmt, ...)` | Info 级别日志 |
| `C2_LOG_WARN(fmt, ...)` | Warning 级别日志 |
| `C2_LOG_ERROR(fmt, ...)` | Error 级别日志（最简洁）|

### 丢弃最旧策略宏

适用于最新日志比完整性更重要的场景（如高频交易行情日志）：

| 宏 | 说明 |
|----|------|
| `C2_LOG_TRACE_DISCARD(fmt, ...)` | Trace（丢弃最旧）|
| `C2_LOG_DEBUG_DISCARD(fmt, ...)` | Debug（丢弃最旧）|
| `C2_LOG_INFO_DISCARD(fmt, ...)` | Info（丢弃最旧）|
| `C2_LOG_WARN_DISCARD(fmt, ...)` | Warn（丢弃最旧）|
| `C2_LOG_ERROR_DISCARD(fmt, ...)` | Error（丢弃最旧）|

### 日志级别

```cpp
enum class LogLevel : int {
    trace = 0,  // 最详细 - 详细追踪
    debug = 1,  // 调试信息
    info = 2,   // 一般信息
    warn = 3,   // 警告状态
    error = 4,  // 错误状态（最简洁）
};
```

## 配置选项

### 缓冲区大小

默认每线程缓冲区为 16MB，可在编译时覆盖：

```cpp
// 在包含 c2_fastlog.h 之前定义
#define FASTLOG_THREAD_BUFFER_SIZE (1 << 26)  // 64MB
#include <c2_fastlog.h>
```

### 自定义 Sink

C2FastLog 使用 spdlog 作为后端，支持所有 spdlog sink：

```cpp
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/daily_file_sink.h>

// 文件 sink
auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>("app.log");

// 滚动文件 sink（最大 5MB，保留 3 个文件）
auto rotating_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
    "app.log", 5*1024*1024, 3);

// 每日滚动文件 sink
auto daily_sink = std::make_shared<spdlog::sinks::daily_file_sink_mt>(
    "app.log", 0, 0);  // 午夜滚动

// 多个 sink
auto logger = std::make_shared<c2fastlog::detail::C2SpdLogger>(
    "multi", spdlog::sinks_init_list{console_sink, file_sink});
```

## 最佳实践

### 1. 在性能关键线程中预分配

```cpp
void trading_thread() {
    // 进入热路径前预分配缓冲区
    c2fastlog::preallocate();

    while (running) {
        // 关键交易逻辑
        // 日志记录不会分配内存
        C2_LOG_INFO("订单执行: id={}, price={}", order.id, order.price);
    }
}
```

### 2. 使用合适的日志级别

```cpp
// 开发环境
c2fastlog::initialize(logger, c2fastlog::LogLevel::trace, ...);

// 生产环境
c2fastlog::initialize(logger, c2fastlog::LogLevel::info, ...);
```

### 3. 避免热路径中的昂贵参数

```cpp
// 不推荐：即使日志被过滤，昂贵的 toString() 也会被调用
C2_LOG_DEBUG("状态: {}", expensive_object.toString());

// 推荐：简单类型捕获成本低
C2_LOG_DEBUG("状态: id={}, status={}", obj.id, obj.status);
```

### 4. 行情数据使用丢弃策略

```cpp
void market_data_handler(const Quote& quote) {
    // 对高频数据使用丢弃最旧策略
    // 最新报价比历史完整性更重要
    C2_LOG_INFO_DISCARD("报价: {}@{}", quote.price, quote.volume);
}
```

### 5. 优雅关闭

```cpp
void shutdown() {
    // 通知线程停止
    running = false;

    // 等待待处理日志刷新
    std::this_thread::sleep_for(std::chrono::seconds(2));

    // 释放资源
    c2fastlog::release();
}
```

## 内存布局

### 线程缓冲区结构

```
线程缓冲区（默认 16MB）:
┌──────────────────────────────────────────────────────────────┐
│ Msg1 │ Msg2 │ Msg3 │ ... │ 填充（如需要）│ ... │             │
└──────────────────────────────────────────────────────────────┘
   │
   ▼
┌─────────────────────────────┐
│ type: uint32_t              │  ← 日志 ID（首次使用时注册）
│ size: uint32_t              │  ← 内容大小（字节）
│ content[]:                  │  ← [时间戳 (8B)][打包参数...]
│   - timestamp: uint64_t     │
│   - arg1, arg2, ...         │
└─────────────────────────────┘
```

### 参数打包

| 类型 | 存储方式 |
|------|----------|
| 整数 | 直接复制（1-8 字节）|
| 浮点数 | 直接复制（4-8 字节）|
| C 字符串 | 长度 (8B) + 数据 + 空字符 |
| std::string | 长度 (8B) + 数据 + 空字符 |
| 自定义类型 | 拷贝构造或 memcpy |

## 性能测试

运行内置基准测试：

```bash
./c2fastlog_benchmark
```

预期输出：
```
=== C2FastLog Benchmark ===

Warming up with 100000 logs...
Running benchmark with 10000000 logs...

=== Results ===
Total logs:      10000000
Total time:      1.74 s
Throughput:      5.73 M logs/sec
Latency:         174.47 ns/log

Waiting for logs to flush...
Benchmark completed!
```

## 项目结构

```
c2fastlog/
├── include/
│   ├── c2_fastlog.h         # 主 API 头文件
│   ├── c2_logger.h          # 核心 FastLogger 实现
│   ├── c2_packer.h          # 参数打包/解包工具
│   ├── c2_spdlog.h          # spdlog 集成
│   ├── c2_variable_queue.h  # SPSC 队列实现
│   ├── c2_common.h          # 通用定义
│   └── utils.h              # 工具函数
├── example/
│   ├── main.cpp             # 使用示例
│   └── benchmark.cpp        # 性能基准测试
├── third_party/
│   └── spdlog/              # spdlog 子模块
├── cmake/
│   └── c2fastlogConfig.cmake.in
├── CMakeLists.txt
├── README.md                # 英文文档
├── README_CN.md             # 中文文档
└── LICENSE
```

## 与其他库对比

| 特性 | C2FastLog | spdlog async | NanoLog |
|------|-----------|--------------|---------|
| 延迟 | ~170ns | ~500ns | ~10ns |
| 吞吐量 | 5.7M/s | 1M/s | 80M/s |
| 格式化位置 | 后台线程 | 后台线程 | 编译期 |
| 纯头文件 | 是 | 是 | 否 |
| C++ 标准 | C++20 | C++11 | C++17 |
| 线程安全 | 是 | 是 | 是 |

*注：NanoLog 通过编译期格式解析实现更低延迟，但需要特殊工具支持。*

## 贡献指南

欢迎贡献！请随时提交 Pull Request。

1. Fork 本仓库
2. 创建特性分支 (`git checkout -b feature/amazing-feature`)
3. 提交更改 (`git commit -m '添加惊人特性'`)
4. 推送到分支 (`git push origin feature/amazing-feature`)
5. 发起 Pull Request

## 许可证

本项目基于 MIT 许可证 - 详见 [LICENSE](LICENSE) 文件。

## 致谢

- [spdlog](https://github.com/gabime/spdlog) - 快速 C++ 日志库
- [fmt](https://github.com/fmtlib/fmt) - 现代格式化库
- [NanoLog](https://github.com/PlatformLab/NanoLog) - 低延迟设计灵感来源

---

<p align="center">
  <b>为高频交易而生</b><br>
  <i>每一纳秒都至关重要</i>
</p>
