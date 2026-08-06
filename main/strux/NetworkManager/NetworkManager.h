#pragma once

#include <atomic>
#include <stdint.h>

#include "WiFiInterface.h"
#include "StruxProvider.h"
#include "InitState.h"
#include "CommandEntry.h"
#include "TypedSettings.h"
#include "Timer.h"

class Stream;

class NetworkManager {
    static constexpr const char* TAG = "NetworkManager";
    /// How long one connect attempt is given before it is retried. There is no
    /// retry LIMIT: a device holding credentials that worked once keeps trying
    /// for as long as it is powered. See the note on AP fallback below.
    static constexpr int StaConnectTimeoutMs = 10000;

    // The AP is for a device that has never been told which network to join —
    // provisioning, not recovery. It is deliberately NOT reachable from the
    // retry path: falling back to it because the configured network was briefly
    // absent tore down the station for good (nothing here re-enters STA without
    // a reboot), so an access point rebooting stranded the device until someone
    // power-cycled it. A missing network is a condition time fixes; wrong
    // credentials are not, and only the second is worth an AP.
    static constexpr const char* DefaultApSsid = "Strux-AP";
    static constexpr const char* DefaultApPassword = ""; // Open network

public:
    explicit NetworkManager(StruxProvider& strux);

    NetworkManager(const NetworkManager&) = delete;
    NetworkManager& operator=(const NetworkManager&) = delete;
    NetworkManager(NetworkManager&&) = delete;
    NetworkManager& operator=(NetworkManager&&) = delete;

    void Init();

    WiFiInterface& wifi();
    const WiFiInterface& wifi() const;

    bool IsAccessPoint() const { return wifi_interface_.IsAP(); }

    /// Is there an address to reach the world with yet? Asked by anything that dials
    /// OUT (the relay), because attempting it before an address exists produces
    /// nothing but a failed connection and the log noise of one.
    bool HasIpv4() const { return wifi_interface_.getStatus().has_ipv4; }

    /// Associated AP's signal strength in dBm. False when there is none to report
    /// (AP mode, or not associated).
    bool GetRssi(int8_t& out) const { return wifi_interface_.GetRssi(out); }

private:
    StruxProvider& strux_;

    InitState initState;
    WiFiInterface wifi_interface_;

    // STA connection state
    char staSsid_[33] = {};
    char staPassword_[65] = {};
    std::atomic<int> staRetryCount_{0};
    std::atomic<bool> staConnected_{false};

    /// Whether the current outage has already been explained. A retry loop that
    /// runs for the lifetime of the device turns "one line per failure" into one
    /// line every ten seconds, forever — so the reason is logged once and the
    /// repeats are counted, reported by the connect that finally succeeds.
    /// See docs/reasoning/2026-08-05-22h33.
    std::atomic<bool> staOutageLogged_{false};

    Timer connectTimer_;

    void HandleNetworkEvent(const NetworkEvent& event);
    void AttemptStaConnect();
    void FallbackToAP();

    // ── WebSocket commands (registered with CommandManager in Init) ──
    RequestError Cmd_WifiScan(CommandContext& ctx);

    inline static CommandEntry commands_[] = {
        { "wifi", "scan", &InvokeCommand<&NetworkManager::Cmd_WifiScan> },
    };

    // ── Settings (registered with SettingsManager in Init) ──
    inline static StringSetting wifiSsid_    { "wifi.ssid",     "WiFi SSID",     "" };
    inline static StringSetting wifiPassword_{ "wifi.password", "WiFi Password", "" };
};
