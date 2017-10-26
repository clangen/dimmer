#pragma once

#include <Windows.h>
#include "Monitor.h"

namespace dimmer {
    class Overlay {
        public:
            Overlay(HINSTANCE instance, Monitor monitor);
            ~Overlay();

            void startTimer();
            void killTimer();

        private:
            static LRESULT CALLBACK windowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

            Monitor monitor;
            UINT_PTR timerId;
            HWND hwnd;
    };
}