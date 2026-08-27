//
// Created by superhuap on 2026/8/25.
//

#ifndef HACKFRAMEWORK_MENU_H
#define HACKFRAMEWORK_MENU_H

#include <windows.h>

namespace menu
{

    class Menu
    {
    public:
        static Menu& GetInstance();

        bool Initialize(HWND hWnd);
        void Render();
        void Shutdown();

        Menu(const Menu&) = delete;
        Menu& operator=(const Menu&) = delete;

    private:
        Menu() = default;
        ~Menu() = default;
    };

} // namespace menu

#endif // HACKFRAMEWORK_MENU_H
