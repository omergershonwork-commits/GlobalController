#pragma once

#if __has_include(<local_config.h>)
#include <local_config.h>
#endif

#ifndef GC_WIFI_SSID
#define GC_WIFI_SSID ""
#endif

#ifndef GC_WIFI_PASSWORD
#define GC_WIFI_PASSWORD ""
#endif

struct DeviceConfig {
    const char* wifiSsid;
    const char* wifiPassword;

    bool hasWifiCredentials() const {
        return wifiSsid != nullptr && wifiSsid[0] != '\0';
    }
};

constexpr DeviceConfig kDeviceConfig{
    GC_WIFI_SSID,
    GC_WIFI_PASSWORD,
};
