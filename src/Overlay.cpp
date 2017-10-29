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

#include "Overlay.h"
#include "Monitor.h"
#include <algorithm>
#include <map>

using namespace dimmer;

#define TIMER_ID 0xdeadbeef

constexpr int timerTickMs = 10;
constexpr wchar_t className[] = L"DimmerOverlayClass";
constexpr wchar_t windowTitle[] = L"DimmerOverlayWindow";

static ATOM overlayClass = 0;
static std::map<HWND, Overlay*> hwndToOverlay;

static void registerClass(HINSTANCE instance, WNDPROC wndProc) {
    if (!overlayClass) {
        WNDCLASS wc = {};
        wc.lpfnWndProc = wndProc;
        wc.hInstance = instance;
        wc.lpszClassName = className;
        overlayClass = RegisterClass(&wc);
    }
}

Overlay::Overlay(HINSTANCE instance, Monitor monitor)
: monitor(monitor)
, timerId(0) {
    registerClass(instance, &Overlay::windowProc);

    this->hwnd =
        CreateWindowEx(
            WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_TRANSPARENT | WS_EX_TOOLWINDOW,
            className,
            windowTitle,
            0,
            0, 0, 0, 0, /* dimens */
            nullptr,
            nullptr,
            instance,
            this);

    hwndToOverlay[this->hwnd] = this;

    SetWindowLong(this->hwnd, GWL_STYLE, 0); /* removes title, borders. */

    this->update(monitor);
}

Overlay::~Overlay() {
    this->killTimer();
    DestroyWindow(this->hwnd);
    hwndToOverlay.erase(hwndToOverlay.find(this->hwnd));
    DeleteObject(this->bgBrush);
}

void Overlay::update(Monitor& monitor) {
    if (this->bgBrush) {
        DeleteObject(this->bgBrush);
    }

    this->bgBrush = CreateSolidBrush(getMonitorColor(monitor, RGB(0, 0, 0)));

    int x = monitor.info.rcMonitor.left;
    int y = monitor.info.rcMonitor.top;
    int width = monitor.info.rcMonitor.right - x;
    int height = monitor.info.rcMonitor.bottom - y;

    float value = getMonitorOpacity(this->monitor);
    value = std::min(1.0f, std::max(0.0f, value));
    BYTE opacity = std::min((BYTE)240, (BYTE)(value * 255.0f));

    SetLayeredWindowAttributes(this->hwnd, 0, opacity, LWA_ALPHA);
    SetWindowPos(this->hwnd, HWND_TOPMOST, x, y, width, height, SWP_FRAMECHANGED | SWP_SHOWWINDOW);

    UpdateWindow(this->hwnd);

    this->startTimer();
}

void Overlay::startTimer() {
    this->killTimer();

    if (isPollingEnabled()) {
        this->timerId = SetTimer(this->hwnd, TIMER_ID, timerTickMs, nullptr);
    }
}

void Overlay::killTimer() {
    KillTimer(this->hwnd, this->timerId);
    this->timerId = 0;
}

LRESULT CALLBACK Overlay::windowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    auto overlay = hwndToOverlay.find(hwnd);

    if (overlay != hwndToOverlay.end()) {
        switch (msg) {
            case WM_PAINT: {
                PAINTSTRUCT ps;
                HDC hdc = BeginPaint(hwnd, &ps);
                FillRect(hdc, &ps.rcPaint, overlay->second->bgBrush);
                EndPaint(hwnd, &ps);
                return 0;
            }

            case WM_TIMER: {
                if (wParam == overlay->second->timerId) {
                    BringWindowToTop(hwnd);
                    return 0;
                }
            }
        }
    }

    return DefWindowProc(hwnd, msg, wParam, lParam);
}