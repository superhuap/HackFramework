//
// Created by superhuap on 2026/11/18.
// Ported from the previous HackFramework iteration.
//

#include "feature/impl/Features.h"

#include <memory>

#include "feature/core/FeatureManager.h"

namespace Feature
{

    void RegisterAll()
    {
        auto& manager = Manager::Get();

        // 开发者在此集中注册功能实例（所有权转移给 Manager）：
        //   manager.Register(std::make_unique<MyFeature>());
        //   manager.Register(std::make_unique<XxxFeature>());
    }

} // namespace Feature
