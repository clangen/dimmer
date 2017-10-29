//////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2007-2017 Casey Langen
//
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
//    * Redistributions of source code must retain the above copyright notice,
//      this list of conditions and the following disclaimer.
//
//    * Redistributions in binary form must reproduce the above copyright
//      notice, this list of conditions and the following disclaimer in the
//      documentation and/or other materials provided with the distribution.
//
//    * Neither the name of the author nor the names of other contributors may
//      be used to endorse or promote products derived from this software
//      without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
//
//////////////////////////////////////////////////////////////////////////////

#include "TrayMenu.h"
#include "Monitor.h"
#include "resource.h"
#include <Commdlg.h>
#include <CommCtrl.h>
#include <shellapi.h>
#include <map>
#include <memory>
#include <algorithm>

using namespace dimmer;

#define WM_TRAYICON (WM_USER + 2000)
#define MENU_ID_EXIT 500
#define MENU_ID_POLL 501
#define MENU_ID_ENABLED 502
#define MENU_ID_MONITOR_BASE 1000
#define MENU_ID_MONITOR_USER 100
#define MENU_ID_MONITOR_COLOR 1

#define MENU_ID_DEFAULTK (MENU_ID_MONITOR_USER + 1)
#define MENU_ID_4500K (MENU_ID_MONITOR_USER + 2)
#define MENU_ID_5000K (MENU_ID_MONITOR_USER + 3)
#define MENU_ID_5500K (MENU_ID_MONITOR_USER + 4)
#define MENU_ID_6000K (MENU_ID_MONITOR_USER + 5)

constexpr wchar_t version[] = L"v0.2";
constexpr wchar_t className[] = L"DimmerTrayMenuClass";
constexpr wchar_t windowTitle[] = L"DimmerTrayMenuWindow";
constexpr int offscreen = -32000;

static ATOM overlayClass = 0;
static HICON trayIcon = nullptr;
static HMENU menu = nullptr;
static std::map<HWND, TrayMenu*> hwndToInstance;

struct ColorProcContext {
    TrayMenu* instance;
    Monitor* monitor;
    CHOOSECOLOR* data;
};

static UINT checked(int temp, int value) {
    return (temp == value) ? MF_CHECKED : MF_UNCHECKED;
}

static HMENU createMenu(HWND hwnd) {
    if (menu) {
        DestroyMenu(menu);
    }

    menu = CreatePopupMenu();

    auto monitors = queryMonitors();
    int i = 1;
    for (auto m : monitors) {
        const int checkedValue = (int) round(getMonitorOpacity(m) * 100.0f);
        UINT_PTR baseId = (MENU_ID_MONITOR_BASE * i++);

        /* "brightness" submenu */
        HMENU brightnessMenu = CreatePopupMenu();
        UINT_PTR menuId;
        for (int j = 0; j < 10; j++) {
            const int currentValue = j * 10;

            const std::wstring title = (j == 0)
                ? L"disabled"
                : std::to_wstring(currentValue) + L"%";

            UINT flags = (currentValue == checkedValue) ? MF_CHECKED : 0;
            menuId = baseId + (j * 10);
            AppendMenu(brightnessMenu, flags, menuId, title.c_str());
        }

        /* "temperature" submenu */
        HMENU tempMenu = CreatePopupMenu();
        const int currentTemp = getMonitorTemperature(m);
        AppendMenu(tempMenu, checked(currentTemp, -1), baseId + MENU_ID_DEFAULTK, L"default");
        AppendMenu(tempMenu, checked(currentTemp, 4500), baseId + MENU_ID_4500K, L"4500k");
        AppendMenu(tempMenu, checked(currentTemp, 5000), baseId + MENU_ID_5000K, L"5000k");
        AppendMenu(tempMenu, checked(currentTemp, 5500), baseId + MENU_ID_5500K, L"5500k");
        AppendMenu(tempMenu, checked(currentTemp, 6000), baseId + MENU_ID_6000K, L"6000k");

        /* brightness, temperature popup */
        HMENU brightTempMenu = CreatePopupMenu();
        AppendMenu(brightTempMenu, MF_POPUP, (UINT_PTR) brightnessMenu, L"brightness");
        AppendMenu(brightTempMenu, MF_POPUP, (UINT_PTR) tempMenu, L"temperature");

        /* main menu */
        UINT submenuEnabled = isDimmerEnabled() ? MF_ENABLED : MF_DISABLED;
        AppendMenu(
            menu,
            MF_POPUP | submenuEnabled,
            reinterpret_cast<UINT_PTR>(brightTempMenu),
            m.getName().c_str());
    }

    bool poll = isPollingEnabled();
    AppendMenu(menu, MF_SEPARATOR, 0, L"-");
    AppendMenu(menu, isDimmerEnabled() ? MF_CHECKED : MF_UNCHECKED, MENU_ID_ENABLED, L"enabled");
    AppendMenu(menu, poll ? MF_CHECKED : MF_UNCHECKED, MENU_ID_POLL, L"dim popups");
    AppendMenu(menu, MF_SEPARATOR, 0, L"-");
    AppendMenu(menu, 0, MENU_ID_EXIT, L"exit");
    return menu;
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
    this->dialogVisible = false;

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

    static const std::wstring title =
        std::wstring(L"dimmer") + L" - " + std::wstring(version);

    ::wcscpy_s(this->iconData.szTip, 255, title.c_str());

    Shell_NotifyIcon(NIM_ADD, &this->iconData);
    this->iconData.uVersion = NOTIFYICON_VERSION;
    Shell_NotifyIcon(NIM_SETVERSION, &this->iconData);
}

void TrayMenu::setPopupMenuChangedCallback(PopupMenuChanged callback) {
    this->popupMenuChanged = callback;
}

LRESULT CALLBACK TrayMenu::windowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_TRAYICON: {
            auto instance = hwndToInstance.find(hwnd)->second;

            if (instance->dialogVisible) {
                return 1;
            }

            auto type = LOWORD(lParam);

            if (type == WM_MBUTTONUP) {
                setDimmerEnabled(!isDimmerEnabled());
                instance->notify();
                return 1;
            }
            if (type == WM_LBUTTONUP || type == WM_RBUTTONUP) {
                if (instance->popupMenuChanged) {
                    instance->popupMenuChanged(true);
                }

                menu = createMenu(instance->hwnd);

                /* SetForegroundWindow + PostMessage(WM_NULL) is a hack to prevent
                "sticky popup menu syndrome." i hate you, win32api. */
                ::SetForegroundWindow(hwnd);

                POINT cursor = {};
                ::GetCursorPos(&cursor);

                /* TPM_RETURNCMD instructs this call to take over the message loop,
                and effectively wait, for the user to make a selection. */
                DWORD id = (DWORD)TrackPopupMenuEx(
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
                else if (id == MENU_ID_ENABLED) {
                    setDimmerEnabled(!isDimmerEnabled());
                    hwndToInstance.find(hwnd)->second->notify();
                }
                else {
                    auto index = (id / MENU_ID_MONITOR_BASE) - 1;
                    auto monitors = queryMonitors();

                    if (monitors.size() <= (size_t)index) {
                        return 1;
                    }

                    auto monitor = monitors[index];
                    auto value = id - (MENU_ID_MONITOR_BASE * (index + 1));

                    if (value >= MENU_ID_DEFAULTK && value <= MENU_ID_6000K) {
                        int temperature = -1;
                        switch (value) {
                            case MENU_ID_4500K: temperature = 4500; break;
                            case MENU_ID_5000K: temperature = 5000; break;
                            case MENU_ID_5500K: temperature = 5500; break;
                            case MENU_ID_6000K: temperature = 6000; break;
                        }
                        setMonitorTemperature(monitor, temperature);
                        hwndToInstance.find(hwnd)->second->notify();
                    }
                    else if (id >= MENU_ID_MONITOR_BASE) {
                        /* if above MENU_ID_MONITOR_USER it's not one of the % toggles */
                        if (value >= MENU_ID_MONITOR_USER) {
                            return 1;
                        }
                        /* else it's a dim% value */
                        else {
                            float opacity = (float)value / 100;
                            setMonitorOpacity(monitor, opacity);
                        }
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