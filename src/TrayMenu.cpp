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
#define COLOR_PICKER_SELECTED_COLOR_HWND_ID 709

constexpr wchar_t version[] = L"v0.2";
constexpr wchar_t className[] = L"DimmerTrayMenuClass";
constexpr wchar_t windowTitle[] = L"DimmerTrayMenuWindow";
constexpr int offscreen = -32000;

static ATOM overlayClass = 0;
static HICON trayIcon = nullptr;
static HMENU menu = nullptr;
static std::map<HWND, TrayMenu*> hwndToInstance;
static std::map<std::wstring, HBITMAP> colorBitmaps;
static COLORREF cachedColors[16];

struct ColorProcContext {
    TrayMenu* instance;
    Monitor* monitor;
    CHOOSECOLOR* data;
};

static void clearBitmapCache() {
    for (auto it : colorBitmaps) {
        DeleteObject(it.second);
    }
    colorBitmaps.clear();
}

static HBITMAP cacheBitmap(HWND hwnd, Monitor& monitor) {
    HDC hdc = GetDC(hwnd);

    HBITMAP bitmap = CreateCompatibleBitmap(hdc, 16, 16);

    HDC hdcMem = CreateCompatibleDC(hdc);
    HBITMAP oldBitmap = (HBITMAP)SelectObject(hdcMem, bitmap);
    RECT size = { 0, 0, 16, 16 };
    HBRUSH brush = CreateSolidBrush(getMonitorColor(monitor));
    FillRect(hdcMem, &size, brush);
    SelectObject(hdcMem, oldBitmap);
    DeleteObject(brush);
    DeleteDC(hdcMem);

    ReleaseDC(hwnd, hdc);

    colorBitmaps[monitor.getId()] = bitmap;
    return bitmap;
}

static HMENU createMenu(HWND hwnd) {
    if (menu) {
        DestroyMenu(menu);
        clearBitmapCache();
    }

    menu = CreatePopupMenu();

    auto monitors = queryMonitors();
    int i = 1;
    for (auto m : monitors) {
        const int checkedValue = (int) round(getMonitorOpacity(m) * 100.0f);

        UINT_PTR baseId = (MENU_ID_MONITOR_BASE * i++);
        UINT_PTR menuId;
        HMENU submenu = CreatePopupMenu();
        for (int j = 0; j < 10; j++) {
            const int currentValue = j * 10;

            const std::wstring title = (j == 0)
                ? L"disabled"
                : std::to_wstring(currentValue) + L"%";

            UINT flags = (currentValue == checkedValue) ? MF_CHECKED : 0;
            menuId = baseId + (j * 10);
            AppendMenu(submenu, flags, menuId, title.c_str());
        }

        AppendMenu(submenu, MF_SEPARATOR, 0, L"-");

        menuId = baseId + MENU_ID_MONITOR_USER + 1;
        HBITMAP bitmap = cacheBitmap(hwnd, m);
        AppendMenu(submenu, 0, menuId, L"color");
        SetMenuItemBitmaps(submenu, menuId, MF_BYCOMMAND, bitmap, bitmap);

        UINT submenuEnabled = isDimmerEnabled() ? MF_ENABLED : MF_DISABLED;
        AppendMenu(
            menu,
            MF_POPUP | submenuEnabled,
            reinterpret_cast<UINT_PTR>(submenu),
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

        memset(cachedColors, 0, sizeof(COLORREF) * 16);
        auto monitors = queryMonitors();
        for (size_t i = 0; i < std::min((size_t) 16, monitors.size()); i++) {
            cachedColors[i] = getMonitorColor(monitors[i]);
        }
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

LRESULT CALLBACK TrayMenu::colorWindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam, UINT_PTR id, DWORD_PTR data) {
    if (msg == WM_PAINT) {
        ColorProcContext* cpc = (ColorProcContext*)data;

        /* allow the dialog to paint itself... */
        LRESULT result = DefSubclassProc(hwnd, msg, wparam, lparam);

        /* then get our HWND and pick the middle point. */
        HDC hdc = GetDC(hwnd);

        RECT rect = {};
        GetWindowRect(hwnd, &rect);

        int x = (rect.right - rect.left) / 2;
        int y = (rect.bottom - rect.top) / 2;

        /* snag the current color, set it, and notify */
        COLORREF bg = GetPixel(hdc, x, y);
        if (bg != CLR_INVALID) {
            setMonitorColor(*cpc->monitor, bg);
            cpc->instance->notify();
        }

        ReleaseDC(hwnd, hdc);

        return result;
    }

    return DefSubclassProc(hwnd, msg, wparam, lparam);
}

UINT_PTR CALLBACK TrayMenu::chooseColorHook(HWND dlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_INITDIALOG) {
        ColorProcContext* cpc = (ColorProcContext*)((CHOOSECOLOR*)lParam)->lCustData;
        Monitor* monitor = (Monitor*)(cpc->monitor);

        RECT monitorRect = monitor->info.rcMonitor;
        RECT dialogRect = {};
        GetWindowRect(dlg, &dialogRect);

        /* center the color chooser in the selected monitor */

        auto xOffset =
            monitorRect.left +
            ((monitorRect.right - monitorRect.left) / 2) -
            ((dialogRect.right - dialogRect.left) / 2);

        auto yOffset =
            monitorRect.top +
            ((monitorRect.bottom - monitorRect.top) / 2) -
            ((dialogRect.bottom - dialogRect.top) / 2);

        SetWindowPos(dlg, 0, xOffset, yOffset, 0, 0, SWP_NOSIZE | SWP_NOACTIVATE);

        /* COLOR_PICKER_SELECTED_COLOR_HWND_ID was discovered with Spy++ */
        HWND colorHwnd = GetDlgItem(dlg, COLOR_PICKER_SELECTED_COLOR_HWND_ID);
        if (colorHwnd) {
            SetWindowSubclass(
                colorHwnd,
                &TrayMenu::colorWindowProc,
                1001,
                (DWORD_PTR)cpc);
        }

        return 0;
    }

    return 0;
}

bool TrayMenu::chooseColor(HWND hwnd, Monitor& monitor, int index, COLORREF& target) {
    ColorProcContext cpc = { 0 };
    CHOOSECOLOR cc = { 0 };

    cc.lStructSize = sizeof(CHOOSECOLOR);
    cc.hwndOwner = nullptr;
    cc.rgbResult = target;
    cc.Flags = CC_RGBINIT | CC_ENABLEHOOK | CC_ANYCOLOR | CC_FULLOPEN;
    cc.lpCustColors = cachedColors;
    cc.lpfnHook = &TrayMenu::chooseColorHook;
    cc.lCustData = (LPARAM)(&cpc);

    cpc.instance = this;
    cpc.monitor = &monitor;
    cpc.data = &cc;

    COLORREF original = getMonitorColor(monitor);

    /* polling screws with our automatic update because it messes
    with Z-order, which causes certain dialog events to get funky.
    turn it off, we'll turn it back on after adjsuting. */
    bool pollingEnabled = isPollingEnabled();
    if (pollingEnabled) {
        setPollingEnabled(false);
        this->notify();
    }

    /* shows the dialog, blocks until complete */
    this->dialogVisible = true;
    BOOL result = ChooseColor(&cc);
    this->dialogVisible = false;

    /* re-enable polling if necessary! */
    if (pollingEnabled) {
        setPollingEnabled(pollingEnabled);
        this->notify();
    }

    if (result) {
        target = cc.rgbResult;
        if (index < 16) {
            cachedColors[index] = cc.rgbResult;
        }
    }
    else {
        target = original;
    }

    return !!result;
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
                else if (id >= MENU_ID_MONITOR_BASE) {
                    auto index = (id / MENU_ID_MONITOR_BASE) - 1;
                    auto monitors = queryMonitors();

                    if (monitors.size() <= (size_t)index) {
                        return 1;
                    }

                    auto monitor = monitors[index];

                    auto value = id - (MENU_ID_MONITOR_BASE * (index + 1));
                    /* if above MENU_ID_MONITOR_USER it's not one of the % toggles */
                    if (value >= MENU_ID_MONITOR_USER) {
                        COLORREF color = getMonitorColor(monitor);
                        if (instance->chooseColor(instance->hwnd, monitor, index, color)) {
                            setMonitorColor(monitor, color);
                        }
                        else {
                            return 1;
                        }
                    }
                    /* else it's a dim% value */
                    else {
                        float opacity = (float)value / 100;
                        setMonitorOpacity(monitor, opacity);
                    }

                    hwndToInstance.find(hwnd)->second->notify();
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