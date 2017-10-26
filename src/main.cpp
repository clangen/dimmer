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

    trayMenu.setPopupMenuChangedCallback([](bool visible) {
        for (auto overlay : overlays) {
            if (visible) {
                overlay.second->killTimer();
            }
            else {
                overlay.second->startTimer();
            }
        }
    });

    MSG msg = {};
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    dimmer::saveConfig();

    monitors.clear();
    overlays.clear();

    return 0;
}

