//
// Created by superhuap on 2026/11/18.
// Ported from the previous HackFramework iteration.
//

#ifndef HACKFRAMEWORK_FEATURES_H
#define HACKFRAMEWORK_FEATURES_H

namespace Feature
{

    // 集中注册所有功能。
    // 在 Menu::Initialize（Manager::Start 之前）被调用。
    // 开发者在此处登记自己的功能实例：
    //   Manager::Get().Register(new MyFeature());
    void RegisterAll();

} // namespace Feature

#endif // HACKFRAMEWORK_FEATURES_H
