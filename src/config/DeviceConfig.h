#pragma once

#include <Arduino.h>
#include <WiFi.h>

#if __has_include(<local_config.h>)
#include <local_config.h>
#endif

#ifndef GC_WIFI_SSID
#define GC_WIFI_SSID ""
#endif

#ifndef GC_WIFI_PASSWORD
#define GC_WIFI_PASSWORD ""
#endif

#ifndef GC_XIAOMI_TV_IP
#define GC_XIAOMI_TV_IP ""
#endif

struct DeviceConfig {
    const char* wifiSsid;
    const char* wifiPassword;
    const char* xiaomiTvIp;

    bool hasWifiCredentials() const {
        return wifiSsid != nullptr &&
               wifiSsid[0] != '\0' &&
               wifiPassword != nullptr &&
               wifiPassword[0] != '\0';
    }

    bool parseXiaomiTvIp(IPAddress& address) const {
        if (xiaomiTvIp == nullptr || xiaomiTvIp[0] == '\0') {
            return false;
        }

        return address.fromString(xiaomiTvIp);
    }
};

constexpr DeviceConfig kDeviceConfig{
    GC_WIFI_SSID,
    GC_WIFI_PASSWORD,
    GC_XIAOMI_TV_IP,
};
