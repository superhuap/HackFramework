//
// Created by superhuap on 2026/11/18.
// Adapted from Universal-Dear-ImGui-Hook (MIT), src/uih.cpp.
//

#ifndef HACKFRAMEWORK_INPUTLOCK_H
#define HACKFRAMEWORK_INPUTLOCK_H

namespace InputLock
{

    // Dear ImGui is not thread-safe. The window procedure feeds its input on
    // the thread of the window while the overlay draws on the thread that
    // presents: both hold this lock while they use the input or create,
    // destroy or rebind the ImGui context.
    class Scoped
    {
    public:
        Scoped();
        ~Scoped();

        Scoped(const Scoped&) = delete;
        Scoped& operator=(const Scoped&) = delete;

    private:
        int depth_;
    };

    // Input locks held by this thread.
    int Depth();
    // Releases the input locks this thread took beyond `depth`.
    void ReleaseTo(int depth);

} // namespace InputLock

#endif // HACKFRAMEWORK_INPUTLOCK_H
