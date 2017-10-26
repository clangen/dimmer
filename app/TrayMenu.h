#pragma once

#include <functional>
#include <Windows.h>

namespace dimmer {
    class TrayMenu {
        using MonitorsChanged = std::function<void ()>;

        public:
            TrayMenu(HINSTANCE instance, MonitorsChanged callback);
            ~TrayMenu();

        private:
            void initIcon();

            void notify() {
                if (monitorsChanged) {
                    monitorsChanged();
                }
            }

            static LRESULT CALLBACK windowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

            HWND hwnd;
            NOTIFYICONDATA iconData;
            MonitorsChanged monitorsChanged;
    };
}