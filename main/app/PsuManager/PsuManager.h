#pragma once

#include "AppProvider.h"
#include "InitState.h"
#include "CommandEntry.h"
#include "TypedSettings.h"
#include "Task.h"
#include "DPS5020.h"

// ──────────────────────────────────────────────────────────────
// The application: a DPS50xx bench supply on the other end of a Modbus wire.
//
// This is the whole product, and it is an application manager like any other —
// nothing in strux/ knows it exists. It announces itself by registering into
// the framework from its own Init(), reaching:
//
//   • the board, for the supply and the LED  — app_.getBoard().GetDps()
//   • the framework, for settings            — psu.poll / psu.telem
//   • the framework, for commands            — `psu get` / `psu set`
//   • the framework, for telemetry           — a point per successful poll
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

    /// Room for a DPS5020 poll, a telemetry Point (~350 bytes of buffers) and the
    /// float formatting that Commit() does.
    static constexpr int TASK_STACK = 5120;
    static constexpr int TASK_PRIORITY = 4;

    // The hardware's full-scale range. Used to refuse a setpoint that the supply
    // would silently clamp — a mistyped 500 V should come back as an error, not
    // as 50 V. This is MEANING validation, which is the handler's job; the
    // framework has already checked that the argument was a number at all.
    static constexpr float MAX_VOLTAGE = 50.0f;
    static constexpr float MAX_CURRENT = 20.0f;
    static constexpr uint32_t MAX_BACKLIGHT = 5;

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
    /// there is stack for it.
    void Record(const DPS5020Data& d);

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

    inline static CommandEntry commands_[] = {
        { "psu", "get", &InvokeCommand<&PsuManager::Cmd_Get> },
        { "psu", "set", &InvokeCommand<&PsuManager::Cmd_Set> },
    };
};
