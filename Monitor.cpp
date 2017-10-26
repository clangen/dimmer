#include "Monitor.h"
#include "Util.h"
#include <map>
#include "json.hpp"

using namespace dimmer;
using namespace nlohmann;

static std::map<std::wstring, float> monitorToOpacity;
static bool pollingEnabled = false;

static std::wstring getConfigFilename() {
    return getDataDirectory() + L"\\config.json";
}

static BOOL CALLBACK MonitorEnumProc(HMONITOR monitor, HDC hdc, LPRECT rect, LPARAM data) {
    auto monitors = reinterpret_cast<std::vector<Monitor>*>(data);
    int index = (int) monitors->size();
    monitors->push_back(Monitor(monitor, index));
    return TRUE;
}

namespace dimmer {
    std::vector<Monitor> queryMonitors() {
        std::vector<Monitor> result;

        EnumDisplayMonitors(
            nullptr,
            nullptr,
            &MonitorEnumProc,
            reinterpret_cast<LPARAM>(&result));

        return result;
    }

    float getMonitorOpacity(Monitor& monitor, float defaultOpacity) {
        auto it = monitorToOpacity.find(monitor.getId());
        auto end = monitorToOpacity.end();

        if (it == end) {
            setMonitorOpacity(monitor, defaultOpacity);
            return defaultOpacity;
        }

        return it->second;
    }

    void setMonitorOpacity(Monitor& monitor, float opacity) {
        monitorToOpacity[monitor.getId()] = opacity;
        saveConfig();
    }

    bool isPollingEnabled() {
        return pollingEnabled;
    }

    void setPollingEnabled(bool enabled) {
        pollingEnabled = enabled;
        saveConfig();
    }

    void loadConfig() {
        std::string config = fileToString(getConfigFilename());
        try {
            json j = json::parse(config);
            auto m = j.find("monitors");
            if (m != j.end()) {
                for (auto it = (*m).begin(); it != (*m).end(); ++it) {
                    std::wstring key = u8to16(it.key());
                    float value = it.value();
                    monitorToOpacity[key] = value;
                }
            }

            auto g = j.find("general");
            if (g != j.end()) {
                pollingEnabled = (*g).value("pollingEnabled", false);
            }
        }
        catch (...) {
            /* move on... */
        }
    }

    void saveConfig() {
        json j = { { "monitors", { } } };

        json& m = j["monitors"];
        for (auto it : monitorToOpacity) {
            m[u16to8(it.first).c_str()] = it.second;
        }

        j["general"] = { { "pollingEnabled", pollingEnabled } };

        stringToFile(getConfigFilename(), j.dump(2));
    }
}