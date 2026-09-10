#pragma once

#include "StruxProvider.h"
#include "InitState.h"
#include "CommandEntry.h"
#include "UiModule.h"
#include "Setting.h"
#include "TypedSettings.h"
#include "Mutex.h"
#include <initializer_list>

// ──────────────────────────────────────────────────────────────
// The registry of UI modules this firmware ships, and the one command that reports it.
//
// `ui modules` is how a shell — the device's own, or the relay's — learns which pages a
// device offers. It is a COMMAND and not a file in `www`, for the same reason `help
// list` and `settings list` are commands: the command surface is how this device
// describes itself, and its HTTP server hands static files to a browser on the LAN and
// does nothing else. Two things fall out of that and both are worth having:
//
//   • one mechanism for both shells — a shell reached through the relay asks the same
//     question the same way, over the pipe it already has,
//   • the answer cannot go stale, because it never passes through the relay's file
//     cache the way a `modules.json` would have.
//
// Nothing here knows what a module *is* beyond its declaration. The bundle is bytes in
// `www` that a shell imports; this manager says which ones exist and what they claim to
// contribute, and the claim is matched against what the module registers on activation.
// ──────────────────────────────────────────────────────────────
class UiManager
{
    static constexpr const char* TAG = "UiManager";

    // The shell/module contract this firmware's bundles were built against. A range
    // rather than a single number so a shell can be newer than the device and still
    // load its modules; a plain integer rather than a list of capability strings
    // because one party owns the firmware and both shells (see docs/reasoning). It
    // versions the whole host contract, the frontend runtime included, so a React major
    // in the shells moves it.
    // 2 since the contract grew `upload`, `download` and `logs`: a firmware module
    // that writes a partition or tails the log cannot run on a shell that speaks 1, so
    // the minimum moved with it. A v1 shell meeting this device says "needs a newer
    // shell" and keeps working otherwise, which is what the range is for.
    static constexpr uint32_t HOST_API_MIN = 2;
    static constexpr uint32_t HOST_API_MAX = 2;

public:
    explicit UiManager(StruxProvider& strux);

    UiManager(const UiManager&) = delete;
    UiManager& operator=(const UiManager&) = delete;
    UiManager(UiManager&&) = delete;
    UiManager& operator=(UiManager&&) = delete;

    void Init();

    /// Called from an application manager's Init(). Entries MUST have static storage
    /// duration — the registry keeps the pointer and reads it on every `ui modules`.
    void Register(std::initializer_list<UiModule*> modules);

private:
    StruxProvider& strux_;
    InitState initState_;
    Mutex mutex_;

    // Whether this device presents its UI as MODULES a shell hosts, or as a whole page
    // a shell merely serves.
    //
    // Both are first-class and the relay supports both, so this is a per-PRODUCT
    // choice rather than a fork of the framework. A product whose frontend is one
    // self-contained app — its own navigation, its own component library, its own
    // charts — sets this false: `ui modules` then answers with an empty array, which
    // is already the documented "no modules, serve the device's whole page" path. The
    // managers still register; nothing below has to know.
    //
    // Runtime rather than compile-time so one build can serve either shape, and so
    // flipping it needs no reflash — which is also what makes the choice reversible.
    //
    // FORK-LOCAL: the default is false here and would be true upstream, because Strux
    // ships four modules and DPS50xx ships none. That single token is the whole of
    // this product's divergence from the framework's UI story — no manager stops
    // registering, no file outside this one changes, and turning it back on is one
    // setting away.
    inline static BoolSetting modulesEnabled_{ "ui.modules", "Serve UI Modules", false };

    /// Intrusive chain, head-inserted — so modules report in reverse registration
    /// order, exactly as settings do. Nothing depends on the order; a shell keys on id.
    UiModule* head_ = nullptr;

    RequestError Cmd_Modules(CommandContext& ctx);

    inline static CommandEntry commands_[] = {
        { "ui", "modules", &InvokeCommand<&UiManager::Cmd_Modules> },
    };
};
