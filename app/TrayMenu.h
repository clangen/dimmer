#pragma once

#include <functional>
#include <Windows.h>

namespace dimmer {
    class TrayMenu {
        using MonitorsChanged = std::function<void()>;
        using PopupMenuChanged = std::function<void(bool)>;

        public:
            TrayMenu(HINSTANCE instance, MonitorsChanged callback);
            ~TrayMenu();

            void setPopupMenuChangedCallback(PopupMenuChanged callback);

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
            PopupMenuChanged popupMenuChanged;
    };
}