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
    /// How long one connect attempt is given before it is retried.
    static constexpr int StaConnectTimeoutMs = 10000;

    /// The device alternates forever: StaAttemptsPerRound attempts at the
    /// configured network, then an AP window, then the same again, for as long as
    /// it is powered. Neither half is terminal, and that is the entire design.
    ///
    /// Both terminal versions were wrong in the same way — each assumed it could
    /// tell, from a failed association, which kind of failure it was looking at.
    /// Ending in the AP stranded a device whose network was merely absent, since
    /// nothing re-entered station mode without a reboot: an access point
    /// rebooting cost a power-cycle. Ending in STA leaves an unattended device
    /// with no way in when the credentials are the thing that is wrong. Cycling
    /// needs to distinguish nothing: whichever failure it is, the half that
    /// addresses it comes round again within fifteen minutes.
    ///
    /// The halves are deliberately lopsided (30 s of STA to a 15 min AP window).
    /// The station round is a machine retrying a local association and needs no
    /// longer; the AP window is the half a human has to notice, walk over to and
    /// use. It is also the half that costs something — DefaultApPassword is
    /// empty, so every window puts an open network back on the air, and on a
    /// device whose web.password is unset that is an unauthenticated console.
    /// Set web.password on anything running outside a lab.
    static constexpr int StaAttemptsPerRound = 3;
    static constexpr int ApWindowMs = 15 * 60 * 1000;

    /// How often a device with no credentials at all re-reads its settings. Not a
    /// window length — nothing is torn down and nothing is attempted, so it is only
    /// how long after pressing Save the AP notices, and fifteen minutes of standing
    /// there is not that.
    static constexpr int ProvisioningPollMs = 30000;

    /// Shown both to a device that has never been told which network to join
    /// (provisioning) and between station rounds (recovery). Credentials are
    /// re-read at the start of every round, so a network provisioned through this
    /// AP is picked up by the next round without a reboot.
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
    std::atomic<bool> staConnected_{false};

    /// Attempts in the current round, and across the whole outage. Only the first
    /// decides anything — when it reaches StaAttemptsPerRound the AP window opens;
    /// the second exists to be reported, by the connect that finally succeeds.
    std::atomic<int> staRoundAttempts_{0};
    std::atomic<int> staOutageAttempts_{0};

    /// When the current outage began. Attempts × timeout used to stand in for how
    /// long the network had been gone, and stopped being that number the moment AP
    /// windows started sitting between the attempts.
    TickType_t staOutageStartTick_ = 0;

    /// Whether the AP is up as the second half of the cycle rather than because
    /// this device has no credentials — only the first kind closes by itself. It
    /// is also what keeps the STA-disconnect path still while the station is being
    /// torn down to open the window, that teardown raising a disconnect of its own
    /// which would otherwise be counted as another failed attempt.
    std::atomic<bool> apWindowOpen_{false};

    /// Whether the current outage has already been explained. A retry loop that
    /// runs for the lifetime of the device turns "one line per failure" into one
    /// line every ten seconds, forever — so the reason is logged once and the
    /// repeats are counted, reported by the connect that finally succeeds.
    /// See docs/reasoning/2026-08-05-22h33.
    std::atomic<bool> staOutageLogged_{false};

    /// One timer, two periods: the patience for a single connect attempt, and the
    /// length of the AP window. Which one is running is apWindowOpen_.
    Timer connectTimer_;

    void HandleNetworkEvent(const NetworkEvent& event);
    void OnCycleTimer();
    void BeginStaRound();
    void AttemptStaConnect();
    void OpenApWindow();
    void StartProvisioningAp();

    // ── WebSocket commands (registered with CommandManager in Init) ──
    RequestError Cmd_WifiScan(CommandContext& ctx);

    inline static CommandEntry commands_[] = {
        { "wifi", "scan", &InvokeCommand<&NetworkManager::Cmd_WifiScan> },
    };

    // ── Settings (registered with SettingsManager in Init) ──
    inline static StringSetting wifiSsid_    { "wifi.ssid",     "WiFi SSID",     "" };
    inline static StringSetting wifiPassword_{ "wifi.password", "WiFi Password", "" };
};
