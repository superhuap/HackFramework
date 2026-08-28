//
// Created by superhuap on 2026/8/25.
//

#ifndef HACKFRAMEWORK_BUFFEREDFEATURE_H
#define HACKFRAMEWORK_BUFFEREDFEATURE_H

#include "features/IFeature.h"

#include "utils/LockFreeBuffer.h"

namespace Feature
{

    // 内置无锁双缓冲的便捷基类。
    //
    // 用法：
    //   class MyFeature : public Feature::BufferedFeature<MySnapshot>
    //   {
    //       void OnUpdate() override { /* 读内存+计算 */ Publish(snap); }
    //       void OnDraw()   override { if (auto s = Read()) { /* ImGui 绘制 */ } }
    //   };
    template <typename Snapshot>
    class BufferedFeature : public IFeature
    {
    protected:
        // OnUpdate（后台线程）里调用：发布一份新快照
        void Publish(const Snapshot& snapshot) { m_buffer.Publish(snapshot); }
        void Publish(Snapshot&& snapshot)      { m_buffer.Publish(std::move(snapshot)); }

        // OnDraw（渲染线程）里调用：获取最新快照，可能为 nullptr
        std::shared_ptr<const Snapshot> Read() { return m_buffer.Read(); }

    private:
        Utils::LockFreeBuffer<Snapshot> m_buffer;
    };

} // namespace Feature

#endif // HACKFRAMEWORK_BUFFEREDFEATURE_H