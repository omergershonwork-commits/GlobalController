#pragma once

// Optional build-time Wi-Fi defaults.
// Copy this file to include/local_config.h only when you prefer build-time
// credentials. local_config.h is ignored by Git and must never be committed.
//
// The Xiaomi TV address is discovered automatically through mDNS, so no IP
// address belongs in this file.

#define GC_WIFI_SSID "YOUR_WIFI_NAME"
#define GC_WIFI_PASSWORD "YOUR_WIFI_PASSWORD"
