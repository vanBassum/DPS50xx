#pragma once

#include "SettingsManager.h"

// ──────────────────────────────────────────────────────────────
// Setting definitions — add new settings here
// ──────────────────────────────────────────────────────────────

inline const SettingDef SETTINGS_DEFS[] = {
    // WiFi
    { "wifi.ssid",     SettingType::String, "WiFi SSID",     "" },
    { "wifi.password", SettingType::String, "WiFi Password", "" },

    // Device
    { "device.name",   SettingType::String, "Device Name",   "DPS50xx" },
    { "device.pin",    SettingType::String, "Device PIN",    "" },

    // Time
    { "ntp.server",    SettingType::String, "NTP Server",    "pool.ntp.org" },
    { "ntp.timezone",  SettingType::String, "Timezone (POSIX)", "CET-1CEST,M3.5.0,M10.5.0/3" },
};

inline constexpr int SETTINGS_DEFS_COUNT = sizeof(SETTINGS_DEFS) / sizeof(SETTINGS_DEFS[0]);
