//
// Created by superhuap on 2026/11/18.
// Adapted from Universal-Dear-ImGui-Hook (MIT), src/uih.cpp.
//

#include "utils/InputLock.h"

#include <mutex>

namespace InputLock
{

    namespace
    {
        std::recursive_mutex g_inputMutex;
        thread_local int t_locks = 0;
    }

    Scoped::Scoped() : depth_(t_locks)
    {
        g_inputMutex.lock();
        ++t_locks;
    }

    Scoped::~Scoped() { ReleaseTo(depth_); }

    int Depth() { return t_locks; }

    void ReleaseTo(int depth)
    {
        for (; t_locks > depth; --t_locks)
            g_inputMutex.unlock();
    }

} // namespace InputLock
