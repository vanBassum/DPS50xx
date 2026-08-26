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
    /// How long one connect attempt is given before it is retried. This is the
    /// ceiling on an attempt that says nothing at all; an attempt that fails out
    /// loud is retried after StaRetryDelayMs instead, because waiting out a timeout
    /// for news that has already arrived is nine idle seconds per attempt.
    static constexpr int StaConnectTimeoutMs = 10000;

    /// How long to leave the radio alone after a failed association before trying
    /// again. Not zero: the failures that clear on their own are the ones where the
    /// AP was momentarily unable to answer, and a retry inside the same instant is
    /// the same instant. Short, because the round is bounded by attempts, not time.
    static constexpr int StaRetryDelayMs = 2000;

    /// How long DHCP gets once the radio has associated. This is a separate wait
    /// from StaConnectTimeoutMs, and it has to be: association and address are two
    /// steps, and the cycle timer used to treat the gap between them as a failed
    /// attempt — rotating networks on a station that was already joined. See the
    /// staAssociated_ note below.
    static constexpr int StaDhcpTimeoutMs = 15000;

    /// How many networks may be configured. Two, because the reason for a second is
    /// a device that lives within reach of two and cannot be told which one will be
    /// up — not a list to be managed, which would want a different settings shape
    /// than one key per field.
    static constexpr int MaxStaNetworks = 2;

    /// The device alternates forever: StaAttemptsPerRound attempts at each
    /// configured network, then an AP window, then the same again, for as long as
    /// it is powered. Neither half is terminal, and that is the entire design.
    ///
    /// The attempts *rotate* between networks rather than exhausting one before
    /// starting the next, which matters in the case the second network exists for:
    /// the first being absent. A round that spends three attempts and eight seconds
    /// establishing that before it tries the network that is actually up has spent
    /// them for nothing, where rotating reaches it on the second attempt.
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

    /// Is there a route off this device yet? Asked by anything that dials OUT (the
    /// relay), because attempting it before there is one produces nothing but a failed
    /// connection and the log noise of one.
    ///
    /// Deliberately not "does an interface have an address": the AP netif is always
    /// 192.168.4.1, so that question answers yes for the whole of a recovery AP window
    /// — the one stretch of time when there is certainly nowhere to dial. It is the
    /// station's address that means the world is reachable, which is what
    /// staConnected_ tracks. See docs/reasoning/2026-08-11-13h05.
    bool HasUpstream() const { return staConnected_ && !wifi_interface_.IsAP(); }

    /// Associated AP's signal strength in dBm. False when there is none to report
    /// (AP mode, or not associated).
    bool GetRssi(int8_t& out) const { return wifi_interface_.GetRssi(out); }

private:
    StruxProvider& strux_;

    InitState initState;
    WiFiInterface wifi_interface_;

    // STA connection state
    struct StaNetwork {
        char ssid[33];
        char password[65];
    };

    /// The configured networks, compacted: an empty SSID is not a network, so
    /// staNetworks_[0..staCount_) are all real and a device configured only through
    /// the second pair of settings is not a device with no network. Re-read at the
    /// start of every round, which is what lets provisioning take effect without a
    /// reboot.
    StaNetwork staNetworks_[MaxStaNetworks] = {};
    int staCount_ = 0;

    /// Which network the next attempt targets, and which one the last attempt used.
    /// The pair is what tells "try that again" from "try the other one" — and the
    /// difference is whether the driver needs a new config or just another connect.
    std::atomic<int> staIndex_{0};
    std::atomic<int> staLastIndex_{-1};

    std::atomic<bool> staConnected_{false};

    /// Attempts in the current round, and across the whole outage. Only the first
    /// decides anything — when it reaches StaAttemptsPerRound per configured network
    /// the AP window opens; the second exists to be reported, by the connect that
    /// finally succeeds.
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

    /// Why the last association failed, and how strong the AP's last beacon was.
    /// Kept because the line that explains an outage is written by the cycle timer,
    /// one tick after the event that knows the answer. Reason 0 means no radio event
    /// arrived at all — the attempt ran out of time in silence, which is a different
    /// report and a different suspicion.
    std::atomic<uint8_t> staLastReason_{0};
    std::atomic<int8_t> staLastRssi_{0};

    /// Whether the current outage has already been explained. A retry loop that
    /// runs for the lifetime of the device turns "one line per failure" into one
    /// line every ten seconds, forever — so the reason is logged once and the
    /// repeats are counted, reported by the connect that finally succeeds.
    /// See docs/reasoning/2026-08-05-22h33.
    std::atomic<bool> staOutageLogged_{false};

    /// Associated, but without an address yet — the step between LinkUp and
    /// Ipv4Acquired. staConnected_ cannot answer this: it means "has an address",
    /// which is what its one caller (HasUpstream) needs, and it is only set by
    /// Ipv4Acquired. Without a flag for the middle state the cycle timer read an
    /// associated station as an attempt still failing and rotated to the next
    /// network — esp_wifi refusing each one with "sta is connected, disconnect
    /// before connecting to new ap", which is the only reason the link survived.
    /// DHCP on a busy AP routinely takes longer than one cycle period.
    std::atomic<bool> staAssociated_{false};

    /// One timer, two periods: the patience for a single connect attempt, and the
    /// length of the AP window. Which one is running is apWindowOpen_.
    Timer connectTimer_;

    void HandleNetworkEvent(const NetworkEvent& event);
    void OnCycleTimer();
    void BeginStaRound();

    /// One attempt at staNetworks_[staIndex_]. `freshRadio` restarts the station from
    /// scratch, which is what a round beginning needs (the radio may be in AP mode)
    /// and what an attempt inside a round must not do — see
    /// WiFiInterface::ReconnectSta.
    void AttemptStaConnect(bool freshRadio);

    /// The network the current attempt is aimed at, for the log. Empty string when
    /// nothing is configured, so a format string is never handed a null.
    const char* CurrentSsid() const;

    /// "3 attempts" or "3 attempts at each of 2 networks", into caller storage. The
    /// one-network wording is the one a single-network device deserves to keep: a
    /// count of one is noise in a line already long enough.
    void DescribeRound(char* buf, size_t len) const;
    void OpenApWindow();
    void StartProvisioningAp();

    // ── WebSocket commands (registered with CommandManager in Init) ──
    RequestError Cmd_WifiScan(CommandContext& ctx);

    inline static CommandEntry commands_[] = {
        { "wifi", "scan", &InvokeCommand<&NetworkManager::Cmd_WifiScan> },
    };

    // ── Settings (registered with SettingsManager in Init) ──
    /// Two networks, tried in the order they are declared. The second pair is
    /// optional and an empty SSID means "not configured" — which is also why the
    /// first pair keeps its original keys: a device already provisioned through
    /// `wifi.ssid` must not need reprovisioning to gain a fallback. NVS allows 15
    /// characters per key, and `wifi.password2` is 14.
    inline static StringSetting wifiSsid_     { "wifi.ssid",      "WiFi SSID",              "" };
    inline static StringSetting wifiPassword_ { "wifi.password",  "WiFi Password",          "" };
    inline static StringSetting wifiSsid2_    { "wifi.ssid2",     "WiFi SSID (fallback)",     "" };
    inline static StringSetting wifiPassword2_{ "wifi.password2", "WiFi Password (fallback)", "" };
};
