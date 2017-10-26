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
#include <ShlObj.h>
#include "Util.h"

namespace dimmer {
    std::string u16to8(const std::wstring& utf16) {
        int size = WideCharToMultiByte(CP_UTF8, 0, utf16.c_str(), -1, 0, 0, 0, 0);
        if (size <= 0) return "";
        char* buffer = new char[size];
        WideCharToMultiByte(CP_UTF8, 0, utf16.c_str(), -1, buffer, size, 0, 0);
        std::string utf8str(buffer);
        delete[] buffer;
        return utf8str;
    }

    std::wstring u8to16(const std::string& utf8) {
        int size = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, 0, 0);
        if (size <= 0) return L"";
        wchar_t* buffer = new wchar_t[size];
        MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, buffer, size);
        std::wstring utf16fn(buffer);
        delete[] buffer;
        return utf16fn;
    }

    std::string fileToString(const std::wstring& fn) {
        FILE* f = _wfopen(fn.c_str(), L"rb");
        std::string result;

        if (!f) {
            return result;
        }

        fseek(f, 0, SEEK_END);
        long len = ftell(f);
        rewind(f);

        if (len > 0) {
            char* bytes = new char[len];
            fread(static_cast<void*>(bytes), len, 1, f);
            result.assign(bytes, len);
            delete[] bytes;
        }

        fclose(f);

        return result;
    }

    bool stringToFile(const std::wstring& fn, const std::string& str) {
        FILE* f = _wfopen(fn.c_str(), L"wb");

        if (!f) {
            return false;
        }

        size_t written = fwrite(str.c_str(), str.size(), 1, f);
        fclose(f);
        return (written == str.size());
    }

    std::wstring getDataDirectory() {
        std::wstring directory;
        DWORD bufferSize = GetEnvironmentVariable(L"APPDATA", 0, 0);
        wchar_t *buffer = new wchar_t[bufferSize + 2];
        GetEnvironmentVariable(L"APPDATA", buffer, bufferSize);
        directory.assign(buffer);
        directory += L"\\dimmer";
        SHCreateDirectoryEx(nullptr, directory.c_str(), nullptr);
        delete[] buffer;
        return directory;
    }
}