#include "Overlay.h"
#include "Monitor.h"
#include <algorithm>
#include <map>
using namespace dimmer;

#define TIMER_ID 0xdeadbeef

constexpr int timerTickMs = 10;
constexpr wchar_t className[] = L"DimmerOverlayClass";
constexpr wchar_t windowTitle[] = L"DimmerOverlayWindow";

static HBRUSH bgBrush = nullptr;
static ATOM overlayClass = 0;
static std::map<HWND, Overlay*> hwndToOverlay;

static void registerClass(HINSTANCE instance, WNDPROC wndProc) {
    if (!bgBrush) {
        bgBrush = CreateSolidBrush(RGB(0, 0, 0));
    }

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

    int x = monitor.info.rcMonitor.left;
    int y = monitor.info.rcMonitor.top;
    int width = monitor.info.rcMonitor.right - x;
    int height = monitor.info.rcMonitor.bottom - y;

    float value = getMonitorOpacity(this->monitor);
    value = std::min(1.0f, std::max(0.0f, value));
    BYTE opacity = std::min((BYTE)240, (BYTE)(value * 255.0f));

    SetLayeredWindowAttributes(this->hwnd, 0, opacity, LWA_ALPHA);
    SetWindowPos(this->hwnd, HWND_TOPMOST, x, y, width, height, SWP_FRAMECHANGED | SWP_SHOWWINDOW);

    this->startTimer();
}

Overlay::~Overlay() {
    this->killTimer();
    DestroyWindow(this->hwnd);
    hwndToOverlay.erase(hwndToOverlay.find(this->hwnd));
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
    switch (msg) {
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            FillRect(hdc, &ps.rcPaint, bgBrush);
            EndPaint(hwnd, &ps);
            return 0;
        }

        case WM_TIMER: {
            auto overlay = hwndToOverlay.find(hwnd);
            auto end = hwndToOverlay.end();
            if (overlay != end && wParam == overlay->second->timerId) {
                BringWindowToTop(hwnd);
                return 0;
            }
        }
    }

    return DefWindowProc(hwnd, msg, wParam, lParam);
}