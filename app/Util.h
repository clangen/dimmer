#pragma once

#include <string>

namespace dimmer {

    extern std::string fileToString(const std::wstring& fn);
    extern bool stringToFile(const std::wstring& fn, const std::string& contents);
    extern std::wstring getDataDirectory();
    extern std::string u16to8(const std::wstring& input);
    extern std::wstring u8to16(const std::string& input);

}