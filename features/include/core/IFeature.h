//
// Created by superhuap on 2026/8/25.
//

#ifndef HACKFRAMEWORK_IFEATURE_H
#define HACKFRAMEWORK_IFEATURE_H

namespace Feature
{

    // 功能抽象接口。
    //
    // 双线程分工模型：
    //   - OnUpdate 在【独立后台线程】执行，每个 feature 拥有独享线程，互不阻塞。
    //     只负责读取游戏数据与计算，把结果 Publish 到共享快照。
    //   - OnDraw   在【渲染线程】执行，只负责 Read() 最新快照并用 ImGui 绘制，不做重活。
    //
    // checkbox 打勾后该功能的后台线程被唤醒执行；不勾选则通过 condition_variable 阻塞挂起。
    // 每次 OnUpdate 执行完后线程 sleep GetUpdateIntervalMs() 毫秒，默认约 3.33ms（300次/秒）。
    class IFeature
    {
    public:
        virtual ~IFeature() = default;

        // 元信息
        virtual const char* GetName() const = 0;

        // 生命周期（由 Feature::Manager 管理）
        //   Initialize 在 Manager::Start 时调用一次；
        //   Shutdown   在 Manager::Stop 时调用一次，用于释放资源。
        virtual bool Initialize() = 0;
        virtual void Shutdown() = 0;

        // 后台线程（每个 feature 独享）：读取 + 计算，Publish 快照（仅启用时被调用）
        virtual void OnUpdate() = 0;

        // 渲染线程：Read() 获取最新快照并绘制（仅勾选时被调用）
        virtual void OnDraw() = 0;

        // checkbox 绑定：返回对勾选状态的引用
        virtual bool& GetEnabled() = 0;

        // 渲染线程：配置面板（颜色、大小等），仅在菜单显示且功能启用时调用
        virtual void DrawOptions() = 0;

        // 后台线程：每次 OnUpdate 执行完后 sleep 的毫秒数
        // 默认 0 表示使用框架默认值（约 3.33ms，即 300次/秒）
        // 返回 > 0 时使用自定义间隔
        virtual float GetUpdateIntervalMs() const { return 0.f; }
    };

} // namespace Feature

#endif // HACKFRAMEWORK_IFEATURE_H