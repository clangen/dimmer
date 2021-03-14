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
#include <Commctrl.h>
#include <string>
#include <vector>
#include <map>
#include <memory>

#include "Monitor.h"
#include "Overlay.h"
#include "TrayMenu.h"
#include "Util.h"

#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

using OverlayPtr = std::shared_ptr<dimmer::Overlay>;
using Overlays = std::map<std::wstring, OverlayPtr>;
static Overlays overlays;
static std::vector<dimmer::Monitor> monitors;

// Interprocess messaging
#define BUF_SIZE 256
TCHAR szName[] = TEXT("DimmerFileMapping");
HANDLE hMapFile;
LPCTSTR pBuf;

// The second one is used so the remote process can talk back to us.
TCHAR szName2[] = TEXT("DimmerFileTalkback");
HANDLE hMapFile2;
LPWSTR pBuf2;

// This custom window message tells the first instance to reload its config file.
const UINT WM_CUSTOM_ARGS = WM_USER + 90;

static void updateOverlays(HINSTANCE instance) {
    monitors = dimmer::queryMonitors();

    Overlays old;
    std::swap(overlays, old);

    if (dimmer::isDimmerEnabled()) {
        for (auto monitor : monitors) {
            auto id = monitor.getId();
            auto it = old.find(monitor.getId());

            OverlayPtr overlay;
            if (it != old.end()) {
                overlay = it->second;
                overlay->update(monitor);
            }
            else {
                overlay = std::make_shared<dimmer::Overlay>(instance, monitor);
            }

            overlays[id] = overlay;
        }
    }
}

// Check to see if this is the first instance by seeing if the file map exists.
static DWORD isFirstInstance() {

    HANDLE hMapFile;
    LPCTSTR pBuf;

    hMapFile = OpenFileMapping(
        FILE_MAP_ALL_ACCESS,   // read/write access
        FALSE,                 // do not inherit the name
        szName);               // name of mapping object

    if (hMapFile == NULL)
    {
        // Must be the first instance
        return 0;
    }

    pBuf = (LPTSTR)MapViewOfFile(hMapFile, // handle to map object
        FILE_MAP_ALL_ACCESS,  // read/write permission
        0,
        0,
        BUF_SIZE);

    if (pBuf == NULL)
    {
        CloseHandle(hMapFile);
        // Still the first instance
        return 0;
    }

    // Convert the string to a thread ID.
    DWORD otherThreadID = _wtoi(pBuf);
    UnmapViewOfFile(pBuf);
    CloseHandle(hMapFile);
    return otherThreadID;
}

// OK, we must be the first instance -- create our two memory mapped files.
static bool makeFirstInstance() {

    hMapFile = CreateFileMapping(
        INVALID_HANDLE_VALUE,    // use paging file
        NULL,                    // default security
        PAGE_READWRITE,          // read/write access
        0,                       // maximum object size (high-order DWORD)
        BUF_SIZE,                // maximum object size (low-order DWORD)
        szName);                 // name of mapping object

    if (hMapFile == NULL)
    {
        //_tprintf(TEXT("Could not create file mapping object (%d).\n"), GetLastError());
        char buffer[100];
        sprintf(buffer, "Could not create file mapping object (%d).\n", GetLastError());
        wchar_t wtext[100];
        mbstowcs(wtext, buffer, strlen(buffer) + 1);//Plus null
        MessageBox(NULL, wtext, TEXT("SETUP"), MB_OK);
        return false;
    }
    pBuf = (LPTSTR)MapViewOfFile(hMapFile,   // handle to map object
        FILE_MAP_ALL_ACCESS, // read/write permission
        0,
        0,
        BUF_SIZE);

    if (pBuf == NULL)
    {
        //_tprintf(TEXT("Could not map view of file (%d).\n"),
        //    GetLastError());

        MessageBox(NULL, TEXT("Could not map view"), TEXT("SETUP"), MB_OK);

        CloseHandle(hMapFile);
        return false;
    }

    // Write my thread ID into the buffer.
    char buffer[100];
    sprintf(buffer, "%d.\n", GetCurrentThreadId());
    wchar_t wtext[100];
    mbstowcs(wtext, buffer, strlen(buffer) + 1);//Plus null
    CopyMemory((PVOID)pBuf, wtext, (strlen(buffer) * sizeof(wchar_t)));

    // Create the second one
    hMapFile2 = CreateFileMapping(
        INVALID_HANDLE_VALUE,    // use paging file
        NULL,                    // default security
        PAGE_READWRITE,          // read/write access
        0,                       // maximum object size (high-order DWORD)
        BUF_SIZE,                // maximum object size (low-order DWORD)
        szName2);                 // name of mapping object

    if (hMapFile2 == NULL)
    {
        return false;
    }
    pBuf2 = (LPTSTR)MapViewOfFile(hMapFile2,   // handle to map object
        FILE_MAP_ALL_ACCESS, // read/write permission
        0,
        0,
        BUF_SIZE);

    if (pBuf2 == NULL)
    {
        CloseHandle(hMapFile2);
        return false;
    }
}

// Close up the handles on exit.
static void CloseMap() {
    UnmapViewOfFile(pBuf);
    CloseHandle(hMapFile);
    UnmapViewOfFile(pBuf2);
    CloseHandle(hMapFile2);
}

// Write the received arguments into the second memory map, for the first instance to consume.
static bool writeArgsBack(LPWSTR args) {

    HANDLE hMapFile;
    LPCTSTR pBuf;

    hMapFile = OpenFileMapping(
        FILE_MAP_ALL_ACCESS,   // read/write access
        FALSE,                 // do not inherit the name
        szName2);               // name of mapping object

    if (hMapFile == NULL)
    {
        // Something went wrong
        return false;
    }

    pBuf = (LPTSTR)MapViewOfFile(hMapFile, // handle to map object
        FILE_MAP_ALL_ACCESS,  // read/write permission
        0,
        0,
        BUF_SIZE);

    if (pBuf == NULL)
    {
        CloseHandle(hMapFile);
        return false;
    }

    // Write the args into it!
    CopyMemory((PVOID)pBuf, args, wcslen(args) * sizeof(LPWSTR));
}

// Parse the incoming application arguments.
void parseArgs(LPWSTR args, HINSTANCE instance) {
    // For now, only a single argument is supported: The name of an alternate config file
    // This config file is loaded and the current config is replaced.
    dimmer::loadConfig(args);

    // Update the screen after the config change.
    updateOverlays(instance);
}

// MAIN function
int CALLBACK wWinMain(HINSTANCE instance, HINSTANCE prev, LPWSTR args, int showType) {
    InitCommonControlsEx(nullptr);

    // See if the app is already running.  If so, exit.
    DWORD otherThread = isFirstInstance();
    if (otherThread > 0) {
        // Write something into the mapping so the first guy picks it up
        if (wcslen(args) > 0) { // Make sure there is actually something to write back
            writeArgsBack(args);
            // Send a message to the other thread.
            PostThreadMessage(otherThread, WM_CUSTOM_ARGS, (WPARAM)0, (LPARAM)0);
        }
        return 0;
    }

    // Set up the file map.
    makeFirstInstance();

    // Load existing config.json off of disk.
    dimmer::loadConfig();

    dimmer::TrayMenu trayMenu(instance, [instance]() {
        updateOverlays(instance);
    });

    // Parse command-line arguments
    if (wcslen(args) > 0) { // ONly if some were passed.
        parseArgs(args, instance);
    }

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
        switch (msg.message)
        {
            case WM_CUSTOM_ARGS:
                // We've received instructions from the other instance of the app.
                parseArgs(pBuf2, instance);

                break;
            default:
                TranslateMessage(&msg);
                DispatchMessage(&msg);
        }
    }

    dimmer::saveConfig();

    monitors.clear();
    overlays.clear();

    CloseMap();

    return 0;
}
