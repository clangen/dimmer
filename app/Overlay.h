#pragma once

#include <Windows.h>
#include "Monitor.h"

namespace dimmer {
    class Overlay {
        public:
            Overlay(HINSTANCE instance, Monitor monitor);
            ~Overlay();

        private:
            Monitor monitor;
            HWND hwnd;
    };
}