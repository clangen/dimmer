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

        std::wstring getId() const {
            return std::wstring(this->info.szDevice) + L"-" + std::to_wstring(index);
        }

        std::wstring getName() const {
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
    extern DWORD getMonitorColor(Monitor& monitor, COLORREF defaultColor = RGB(0, 0, 0));
    extern void setMonitorColor(Monitor& monitor, COLORREF color);
    extern bool isPollingEnabled();
    extern void setPollingEnabled(bool enabled);
    extern bool isDimmerEnabled();
    extern void setDimmerEnabled(bool enabled);
    extern void loadConfig();
    extern void saveConfig();
}