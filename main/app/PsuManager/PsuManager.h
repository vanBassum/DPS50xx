#pragma once

#include "AppProvider.h"
#include "InitState.h"
#include "CommandEntry.h"
#include "TypedSettings.h"
#include "UiModule.h"
#include "Task.h"
#include "interfaces/Psu.h"

// ──────────────────────────────────────────────────────────────
// The application: a bench supply on the other end of a Modbus wire.
//
// This is the whole product, and it is an application manager like any other —
// nothing in strux/ knows it exists. It announces itself by registering into
// the framework from its own Init(), reaching:
//
//   • the board, for the supply and the LED  — app_.getBoard().GetPsu()
//   • the framework, for settings            — psu.poll / psu.telem
//   • the framework, for commands            — `psu get` / `psu set`
//   • the framework, for telemetry           — a point per successful poll
//
// It names no register and no supply. Every value it reports, records or
// validates is a CAPABILITY off the Psu role's chain — semantic by the time it
// arrives, because normalizing a device's storage into meaning happened in the
// driver. That is why an XY6020L's energy counters and its runtime appear in
// `psu get`, in the telemetry and in the dashboard without a line changing
// here. The handful the PRODUCT knows by meaning — the setpoints it writes, the
// values it logs — go through the well-known keys in PsuKey.
//
// The polling lives in a Task rather than a Timer because a Modbus transaction
// blocks for up to its timeout, and blocking in the FreeRTOS timer service task
// is how that task deadlocks against its own command queue. The task's stack is
// also sized for a telemetry Point plus float formatting, which a timer
// callback's stack would not hold.
// ──────────────────────────────────────────────────────────────
class PsuManager
{
    static constexpr const char* TAG = "PsuManager";

    /// The supply's own MCU also drives its display and keypad, so it needs a
    /// moment after power-on before it answers on the bus at all.
    static constexpr int STARTUP_DELAY_MS = 2000;

    /// Room for a poll, a telemetry Point (~350 bytes of buffers) and the float
    /// formatting that Commit() does.
    static constexpr int TASK_STACK = 5120;
    static constexpr int TASK_PRIORITY = 4;

public:
    explicit PsuManager(AppProvider& app);

    PsuManager(const PsuManager&) = delete;
    PsuManager& operator=(const PsuManager&) = delete;
    PsuManager(PsuManager&&) = delete;
    PsuManager& operator=(PsuManager&&) = delete;

    void Init();

private:
    AppProvider& app_;
    InitState initState_;
    Task pollTask_;

    void PollLoop();

    /// One telemetry point per successful poll, taken on the poll task where
    /// there is stack for it. Fields are whatever CAPABILITIES the driver gave a
    /// telemetry name; nothing here lists them, and no register reaches it.
    void Record(Psu& psu);

    /// The status LED mirrors the Modbus link: lit means the supply answered the
    /// last poll. It is the only thing on this board that can say so without a
    /// browser attached.
    void ShowLinkState(bool online);

    bool lastOnline_ = false;

    // ── Settings. A key is at most 15 characters — NVS's limit, asserted at
    // RUNTIME in Register(), so an over-long key compiles fine and then
    // boot-loops the device. "psu.telemetry" would fit; "psu.telem" is shorter
    // to type and matches the framework's own `telem.*` keys.
    inline static UInt32Setting pollIntervalMs_{ "psu.poll", "Poll Interval (ms)", 1000 };
    inline static BoolSetting   telemetry_{ "psu.telem", "Record PSU Telemetry", true };

    // ── Commands ──
    RequestError Cmd_Get(CommandContext& ctx);
    RequestError Cmd_Set(CommandContext& ctx);

    /// Write ONE reading by key, validated against that reading's own range.
    /// This is what makes a supply's long tail — protection thresholds and
    /// whatever the next supply adds — configurable without a new argument
    /// here per register. `psu set` keeps the core setpoints, because those
    /// need a specific ORDER that a generic write cannot express.
    RequestError Cmd_Write(CommandContext& ctx);

    // ── UI. The browser half of this manager is a self-contained ES module in
    // `www`, which a shell imports on demand. What follows is only the
    // *declaration* a shell reads first, so it can draw its navigation without
    // loading a byte of module code — and the id is what the module's own
    // activate() is matched against.
    //
    // One page, and it is the FIRST module this firmware registers: the app's
    // managers Init() after the framework's, and UiManager head-inserts, so the
    // supply lands ahead of console/settings/firmware. That ordering is how a
    // device says which of its features is the product.
    inline static const UiPage uiPages_[] = { { "psu", "Supply", "zap" } };
    inline static UiModule uiModule_{ "psu", "/modules/psu.js", uiPages_ };

    inline static CommandEntry commands_[] = {
        { "psu", "get", &InvokeCommand<&PsuManager::Cmd_Get> },
        { "psu", "set", &InvokeCommand<&PsuManager::Cmd_Set> },
        { "psu", "write", &InvokeCommand<&PsuManager::Cmd_Write> },
    };
};
