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