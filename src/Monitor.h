#pragma once

#include <Windows.h>
#include <vector>
#include <string>

namespace dimmer {
    struct Monitor {
        Monitor(HMONITOR handle, int index) {
            this->handle = handle;
            this->index = index;
            this->info = {};
            this->info.cbSize = sizeof(MONITORINFOEX);
            GetMonitorInfo(handle, &this->info);
        }

        std::wstring getId() {
            return std::wstring(this->info.szDevice) + L"-" + std::to_wstring(index);
        }

        std::wstring getName() {
            std::wstring name = this->info.szDevice;
            auto pos = name.find(L"\\\\.\\");
            if (pos == 0) {
                name = name.substr(4);
            }
            return name;
        }

        int index;
        HMONITOR handle;
        MONITORINFOEX info;
    };

    extern std::vector<Monitor> queryMonitors();
    extern float getMonitorOpacity(Monitor& monitor, float defaultOpacity = 0.30f);
    extern void setMonitorOpacity(Monitor& monitor, float opacity);
    extern bool isPollingEnabled();
    extern void setPollingEnabled(bool enabled);
    extern void loadConfig();
    extern void saveConfig();
}