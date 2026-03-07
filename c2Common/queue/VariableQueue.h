#pragma once

#include <cstdint>
#include <cstring>

#include "c2Common/Utils.h"

namespace c2logger {

struct VariableMessage
{
    uint32_t Type = 0;
    uint32_t Size = 0; // Size 为 0 时为填充块, 下一块直接跳到 0 号块.
    uint8_t Content[];
};

// SPSCVariableQueue 是线程安全变长消息的 SPSC 队列, SIZE 必须大于两倍最大消息长度.
template<uint32_t SIZE>
class SPSCVariableQueue
{
public:
    static constexpr uint32_t MsgHeaderSize = sizeof( VariableMessage );
    static constexpr uint32_t Epsilison = MsgHeaderSize - 1;
    static constexpr uint32_t Capaity = SIZE / MsgHeaderSize;

    // 获取新消息写入位置, 其中 VariableMessage::content 长度为 content_size.
    // 获取成功后返回新消息写入位置, 失败则返回 nullptr.
    // 获取成功后, 在 writeCommit 之前调用 push 或者 allocate 等写入函数将导致指针失效.
    inline VariableMessage* allocate( uint32_t content_size )
    {
        auto allocBlock = ( MsgHeaderSize + content_size + Epsilison ) / MsgHeaderSize;
        if( unlikely( allocBlock >= m_freeBlock ) )
        {
            // 根据读写指针位置更新 m_freeBlock.
            auto tailCache = m_tail;
            if( m_head >= tailCache )
            {
                // 循环队列中, 读写指针在一循环, 写指针在读指针的右边。
                // m_data: |_|_|_|_|_|_|_|_|_|
                //             m_tail---↑ m_head---↑
                m_freeBlock = Capaity - m_head;
                if( allocBlock >= m_freeBlock && tailCache != 0 )
                {
                    // 剩余空间不满足分配需求, 队列进入下一循环, 将 Size 置为 0
                    // 作为填充块, 表明右边不再有数据。写指针 m_head 置为 0 从头开始写入.
                    m_data[m_head].Type = 0;
                    m_data[m_head].Size = 0;
                    m_head = 0;
                    m_freeBlock = tailCache;
                }
            }
            else
            {
                // 循环队列中, 写指针进入下一循环, 读指针在写指针的右边。
                // m_data: |_|_|_|_|_|_|_|_|_|
                //             m_head---↑ m_tail---↑
                m_freeBlock = tailCache - m_head;
            }
            if( allocBlock >= m_freeBlock )
            {
                return nullptr;
            }
        }
        m_data[m_head].Type = 0;
        m_data[m_head].Size = content_size;
        return &m_data[m_head];
    }

    inline void writeCommit()
    {
        auto allocBlock = ( MsgHeaderSize + m_data[m_head].Size + Epsilison ) / MsgHeaderSize;
        if( m_freeBlock > Capaity )
        {
            m_freeBlock = Capaity;
        }
        m_freeBlock -= allocBlock;
        m_head = m_head + allocBlock;
    }

    inline const VariableMessage* front()
    {
        auto headCache = m_head;
        if( m_tail != headCache && m_data[m_tail].Size == 0 )
        {
            m_tail = 0;
        }
        if( m_tail == headCache )
        {
            return nullptr;
        }
        return &m_data[m_tail];
    }

    inline void readCommit()
    {
        if( m_tail == m_head )
        {
            return;
        }
        m_tail = m_tail + ( MsgHeaderSize + m_data[m_tail].Size + Epsilison ) / MsgHeaderSize;
        // 检查当前块是否为填充块, 遇到填充块时读指针需要跳回 0 点.
        if( m_tail != m_head && m_data[m_tail].Size == 0 )
        {
            m_tail = 0;
        }
    }

    inline bool empty()
    {
        return m_tail == m_head;
    }

    // 丢弃最旧消息的分配策略。当 buffer 满时，丢弃最旧的消息直到有足够空间。
    // 警告: 此函数会修改 m_tail，不适用于多线程 SPSC 场景！
    // 仅在单线程场景或确保消费者线程已停止时使用。
    // 返回值: 成功分配的指针，或 nullptr（队列容量不足以容纳单条消息）
    inline VariableMessage* allocateDiscardOldest( uint32_t content_size )
    {
        auto allocBlock = ( MsgHeaderSize + content_size + Epsilison ) / MsgHeaderSize;

        // 检查单条消息是否超过队列最大容量
        if( unlikely( allocBlock >= Capaity ) )
        {
            return nullptr;
        }

        // 尝试正常分配
        auto ptr = allocate( content_size );
        if( ptr != nullptr )
        {
            return ptr;
        }

        // 分配失败，需要丢弃最旧的消息
        // 循环丢弃直到有足够空间
        while( ptr == nullptr )
        {
            // 检查队列是否为空（已丢弃所有消息）
            if( m_tail == m_head )
            {
                // 队列为空，重置指针
                m_head = 0;
                m_tail = 0;
                m_freeBlock = Capaity;
                break;
            }

            // 丢弃最旧的一条消息（移动 tail 指针）
            discardOldestOne();

            // 重新尝试分配
            ptr = allocate( content_size );
        }

        if( ptr == nullptr )
        {
            // 队列已清空后重新分配
            ptr = allocate( content_size );
        }

        return ptr;
    }

private:
    // 丢弃队列中最旧的一条消息（仅供 allocateDiscardOldest 内部使用）
    inline void discardOldestOne()
    {
        if( m_tail == m_head )
        {
            return;
        }

        // 检查是否为填充块
        if( m_data[m_tail].Size == 0 )
        {
            m_tail = 0;
            return;
        }

        // 计算当前消息占用的块数
        auto tailBlock = ( MsgHeaderSize + m_data[m_tail].Size + Epsilison ) / MsgHeaderSize;
        m_tail = m_tail + tailBlock;

        // 检查移动后是否遇到填充块
        if( m_tail != m_head && m_data[m_tail].Size == 0 )
        {
            m_tail = 0;
        }
    }

private:
    volatile size_t m_head = 0;                                     // 队列头, 写指针
    size_t          m_freeBlock = Capaity;                          // 队列中空闲块数量
    alignas( CacheLineSize ) volatile size_t m_tail = 0; // 队列尾, 读指针
    alignas( CacheLineSize ) VariableMessage m_data[Capaity];
};

} // namespace c2logger

