#include "TrayMenu.h"
#include "Monitor.h"
#include "resource.h"
#include <shellapi.h>
#include <map>

using namespace dimmer;

#define WM_TRAYICON (WM_USER + 2000)
#define MENU_ID_EXIT 500
#define MENU_ID_POLL 501
#define MENU_ID_MONITOR_BASE 1000

constexpr wchar_t className[] = L"DimmerTrayMenuClass";
constexpr wchar_t windowTitle[] = L"DimmerTrayMenuWindow";
constexpr int offscreen = -32000;
static ATOM overlayClass = 0;
static HICON trayIcon = nullptr;
static HMENU menu = nullptr;
static std::map<HWND, TrayMenu*> hwndToInstance;

static HMENU createMenu() {
    if (menu) {
        DestroyMenu(menu);
    }

    menu = CreatePopupMenu();

    auto monitors = queryMonitors();
    int i = 1;
    for (auto m : monitors) {
        const int checkedValue = (int) round(getMonitorOpacity(m) * 100.0f);

        UINT_PTR baseId = (MENU_ID_MONITOR_BASE * i++);
        HMENU submenu = CreatePopupMenu();
        for (int j = 0; j < 10; j++) {
            const int currentValue = j * 10;

            const std::wstring title = (j == 0)
                ? L"off"
                : std::to_wstring(currentValue) + L"%";

            UINT flags = (currentValue == checkedValue) ? MF_CHECKED : 0;
            AppendMenu(submenu, flags, baseId + (j * 10), title.c_str());
        }

        AppendMenu(
            menu,
            MF_POPUP,
            reinterpret_cast<UINT_PTR>(submenu),
            m.getName().c_str());
    }

    bool poll = isPollingEnabled();
    AppendMenu(menu, MF_SEPARATOR, 0, L"-");
    AppendMenu(menu, poll ? MF_CHECKED : MF_UNCHECKED, MENU_ID_POLL, L"dim popups");
    AppendMenu(menu, MF_SEPARATOR, 0, L"-");
    AppendMenu(menu, 0, MENU_ID_EXIT, L"exit");
    return menu;
}

LRESULT CALLBACK TrayMenu::windowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_TRAYICON: {
            auto type = LOWORD(lParam);
            if (type == WM_LBUTTONUP || type == WM_RBUTTONUP) {
                auto instance = hwndToInstance.find(hwnd)->second;
                if (instance->popupMenuChanged) {
                    instance->popupMenuChanged(true);
                }

                menu = createMenu();

                /* SetForegroundWindow + PostMessage(WM_NULL) is a hack to prevent
                "sticky popup menu syndrome." i hate you, win32api. */
                ::SetForegroundWindow(hwnd);

                POINT cursor = { };
                ::GetCursorPos(&cursor);

                /* TPM_RETURNCMD instructs this call to take over the message loop,
                and effectively wait, for the user to make a selection. */
                DWORD id = (DWORD) TrackPopupMenuEx(
                    menu, TPM_RETURNCMD, cursor.x, cursor.y, hwnd, nullptr);

                PostMessage(hwnd, WM_NULL, 0, 0);

                /* process the selection... */
                if (id == MENU_ID_EXIT) {
                    PostQuitMessage(0);
                    return 1;
                }
                else if (id == MENU_ID_POLL) {
                    setPollingEnabled(!isPollingEnabled());
                    hwndToInstance.find(hwnd)->second->notify();
                }
                else if (id >= MENU_ID_MONITOR_BASE) {
                    auto index = (id / MENU_ID_MONITOR_BASE) - 1;
                    auto value = id - (MENU_ID_MONITOR_BASE * (index + 1));
                    float opacity = (float)value / 100;
                    auto monitors = queryMonitors();
                    if (monitors.size() > (size_t)index) {
                        auto monitor = monitors[index];
                        setMonitorOpacity(monitor, opacity);
                        hwndToInstance.find(hwnd)->second->notify();
                    }
                }

                if (instance->popupMenuChanged) {
                    instance->popupMenuChanged(false);
                }
            }
            return 0;
        }

        case WM_DISPLAYCHANGE: {
            hwndToInstance.find(hwnd)->second->notify();
            break;
        }
    }

    return DefWindowProc(hwnd, msg, wParam, lParam);
}

static void registerClass(HINSTANCE instance, WNDPROC wndProc) {
    if (!trayIcon) {
        trayIcon = (HICON) ::LoadIconW(
            GetModuleHandle(nullptr),
            MAKEINTRESOURCE(IDI_TRAY_ICON));
    }

    if (!overlayClass) {
        WNDCLASS wc = {};
        wc.lpfnWndProc = wndProc;
        wc.hInstance = instance;
        wc.lpszClassName = className;
        overlayClass = RegisterClass(&wc);
    }
}

TrayMenu::TrayMenu(HINSTANCE instance, MonitorsChanged callback) {
    this->monitorsChanged = callback;

    registerClass(instance, &windowProc);

    this->hwnd =
        CreateWindowEx(
            WS_EX_TOOLWINDOW,
            className,
            windowTitle,
            0,
            0, 0, 0, 0, /* dimens */
            nullptr,
            nullptr,
            instance,
            this);

    hwndToInstance[hwnd] = this;

    SetWindowLong(this->hwnd, GWL_STYLE, 0); /* removes title, borders. */

    SetWindowPos(
        this->hwnd,
        nullptr,
        offscreen, offscreen, 50, 50,
        SWP_FRAMECHANGED | SWP_SHOWWINDOW);

    this->initIcon();

    this->notify();
}

TrayMenu::~TrayMenu() {
    Shell_NotifyIcon(NIM_DELETE, &this->iconData);
    DestroyWindow(this->hwnd);

    auto it = hwndToInstance.find(this->hwnd);
    if (it != hwndToInstance.end()) {
        hwndToInstance.erase(it);
    }
}

void TrayMenu::initIcon() {
    this->iconData = {};
    this->iconData.hWnd = this->hwnd;
    this->iconData.uID = 0;
    this->iconData.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    this->iconData.uCallbackMessage = WM_TRAYICON;
    this->iconData.hIcon = trayIcon;

    static const std::wstring title = L"dimmer";
    ::wcscpy_s(this->iconData.szTip, 255, title.c_str());

    Shell_NotifyIcon(NIM_ADD, &this->iconData);
    this->iconData.uVersion = NOTIFYICON_VERSION;
    Shell_NotifyIcon(NIM_SETVERSION, &this->iconData);
}

void TrayMenu::setPopupMenuChangedCallback(PopupMenuChanged callback) {
    this->popupMenuChanged = callback;
}
