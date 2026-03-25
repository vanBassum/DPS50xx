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

    // MQTT
    { "mqtt.enabled",  SettingType::Bool,   "MQTT Enabled",  "0" },
    { "mqtt.broker",   SettingType::String, "MQTT Broker",   "" },
    { "mqtt.port",     SettingType::Int,    "MQTT Port",     "1883" },
    { "mqtt.user",     SettingType::String, "MQTT Username", "" },
    { "mqtt.pass",     SettingType::String, "MQTT Password", "" },
    { "mqtt.prefix",   SettingType::String, "MQTT Topic Prefix", "dps50xx" },
};

inline constexpr int SETTINGS_DEFS_COUNT = sizeof(SETTINGS_DEFS) / sizeof(SETTINGS_DEFS[0]);
