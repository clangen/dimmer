#include <Windows.h>
#include <string>
#include <vector>
#include <map>
#include <memory>

#include "Monitor.h"
#include "Overlay.h"
#include "TrayMenu.h"
#include "Util.h"

using OverlayPtr = std::shared_ptr<dimmer::Overlay>;
static std::map<std::wstring, OverlayPtr> overlays;
static std::vector<dimmer::Monitor> monitors;

int CALLBACK wWinMain(HINSTANCE instance, HINSTANCE prev, LPWSTR args, int showType) {
    dimmer::loadConfig();

    dimmer::TrayMenu trayMenu(instance, [instance]() {
        monitors = dimmer::queryMonitors();
        overlays.clear();

        for (auto monitor : monitors) {
            if (dimmer::getMonitorOpacity(monitor) > 0.0f) {
                auto overlay = std::make_shared<dimmer::Overlay>(instance, monitor);
                std::wstring name = monitor.info.szDevice;
                overlays[name] = overlay;
            }
        }
    });

    MSG msg = { };
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    dimmer::saveConfig();

    return 0;
}

