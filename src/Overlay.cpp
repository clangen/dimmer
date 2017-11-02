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
static WORD gammaRamp[3][256];

static void registerClass(HINSTANCE instance, WNDPROC wndProc) {
    if (!overlayClass) {
        WNDCLASS wc = {};
        wc.lpfnWndProc = wndProc;
        wc.hInstance = instance;
        wc.lpszClassName = className;
        overlayClass = RegisterClass(&wc);
    }
}

static bool enabled(Monitor& monitor) {
    return isDimmerEnabled() && isMonitorEnabled(monitor);
}

Overlay::Overlay(HINSTANCE instance, Monitor monitor)
: instance(instance)
, monitor(monitor)
, timerId(0)
, bgBrush(CreateSolidBrush(RGB(0, 0, 0)))
, hwnd(nullptr) {
    registerClass(instance, &Overlay::windowProc);
    this->update(monitor);
}

Overlay::~Overlay() {
    this->disableColorTemperature();
    this->disableBrigthnessOverlay();
    DeleteObject(this->bgBrush);
}

static void colorTemperatureToRgb(int kelvin, float& red, float& green, float& blue) {
    kelvin /= 100;

    if (kelvin <= 66) {
        red = 255;
    }
    else {
        red = kelvin - 60.0f;
        red = (float)(329.698727446 * (pow(red, -0.1332047592)));
        red = std::max(0.0f, std::min(255.0f, red));
    }

    if (kelvin <= 66) {
        green = (float) kelvin;
        green = 99.4708025861f * log(green) - 161.1195681661f;
        green = std::max(0.0f, std::min(255.0f, green));
    }
    else {
        green = kelvin - 60.0f;
        green = (float)(288.1221695283 * (pow(green, -0.0755148492)));
        green = std::max(0.0f, std::min(255.0f, green));
    }

    if (kelvin >= 66) {
        blue = 255.0f;
    }
    else {
        blue = kelvin - 10.0f;
        blue = 138.5177312231f * log(blue) - 305.0447927307f;
        blue = std::max(0.0f, std::min(255.0f, blue));
    }

    red /= 255.0f;
    green /= 255.0f;
    blue /= 255.0f;
}

void Overlay::disableColorTemperature() {
    HDC dc = CreateDC(nullptr, monitor.info.szDevice, nullptr, nullptr);
    if (dc) {
        for (int i = 0; i < 256; i++) {
            gammaRamp[0][i] = gammaRamp[1][i] = gammaRamp[2][i] = i * 256;
        }
        SetDeviceGammaRamp(dc, gammaRamp);
        DeleteDC(dc);
    }
}

void Overlay::updateColorTemperature() {
    int temperature = getMonitorTemperature(monitor);

    if (!enabled(monitor) || temperature == -1) {
        disableColorTemperature();
    }
    else {
        HDC dc = CreateDC(nullptr, monitor.info.szDevice, nullptr, nullptr);
        if (dc) {
            float red = 1.0f;
            float green = 1.0f;
            float blue = 1.0f;

            temperature = std::min(6000, std::max(4500, temperature));
            colorTemperatureToRgb(temperature, red, green, blue);

            for (int i = 0; i < 256; i++) {
                float brightness = i * (256.0f);
                gammaRamp[0][i] = (short)std::max(0.0f, std::min(65535.0f, brightness * red));
                gammaRamp[1][i] = (short)std::max(0.0f, std::min(65535.0f, brightness * green));
                gammaRamp[2][i] = (short)std::max(0.0f, std::min(65535.0f, brightness * blue));
            }

            SetDeviceGammaRamp(dc, gammaRamp);

            DeleteDC(dc);
        }
    }
}

void Overlay::disableBrigthnessOverlay() {
    this->killTimer();
    if (this->hwnd) {
        DestroyWindow(this->hwnd);
        hwndToOverlay.erase(hwndToOverlay.find(this->hwnd));
        this->hwnd = nullptr;
    }
}

void Overlay::updateBrightnessOverlay() {
    if (!enabled(monitor) || getMonitorOpacity(monitor) == 0.0f) {
        disableBrigthnessOverlay();
    }
    else {
        if (!this->hwnd) {
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
        }

        float red, green, blue;
        colorTemperatureToRgb(3000, red, green, blue);
        red = (1.0f - red) * 256.0f;
        green = (1.0f - green) * 256.0f;
        blue = (1.0f - blue) * 256.0f;

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
}

void Overlay::update(Monitor& monitor) {
    this->monitor = monitor;
    this->updateColorTemperature();
    this->updateBrightnessOverlay();
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