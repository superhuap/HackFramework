//
// Created by superhuap on 2026/8/25.
//

#ifndef HACKFRAMEWORK_LOCKFREEBUFFER_H
#define HACKFRAMEWORK_LOCKFREEBUFFER_H

#include <atomic>
#include <memory>

namespace Utils
{

    // 单写者(后台线程) + 单读者(渲染线程) 的无锁双缓冲。
    // 写者 Publish 一份值语义快照(深拷贝)，原子交换指针；读者 Read 拿到最新不可变快照。
    // 读端零拷贝、零等待，适用于"后台读取计算，渲染线程绘制"的解耦场景。
    template <typename T>
    class LockFreeBuffer
    {
    public:
        LockFreeBuffer() = default;

        LockFreeBuffer(const LockFreeBuffer&) = delete;
        LockFreeBuffer& operator=(const LockFreeBuffer&) = delete;

        void Publish(const T& snapshot)
        {
            m_current.store(std::make_shared<const T>(snapshot), std::memory_order_release);
        }

        void Publish(T&& snapshot)
        {
            m_current.store(std::make_shared<const T>(std::move(snapshot)), std::memory_order_release);
        }

        // 返回最新快照；若尚未 Publish 过则为 nullptr，调用方需要判空。
        std::shared_ptr<const T> Read() const
        {
            return std::atomic_load_explicit(&m_current, std::memory_order_acquire);
        }

    private:
        std::atomic<std::shared_ptr<const T>> m_current{nullptr};
    };

} // namespace Utils

#endif // HACKFRAMEWORK_LOCKFREEBUFFER_H