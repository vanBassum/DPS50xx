#pragma once

#include "InitState.h"
#include "BoardConfig.h"
#include "interfaces/BoardProvider.h"
#include "drivers/GpioLed.h"
#include "drivers/DPS5020.h"
#include "ModbusRtuClient.h"
#include "ModbusMaster.h"

// ──────────────────────────────────────────────────────────────
// The board layer's context for an ESP32 DevKit driving a DPS50xx supply:
// owns every driver instance and the Modbus bus host, and answers
// BoardProvider.
//
// The bottom layer, depending on nothing above it. Drivers take their pins and
// buses as constructor arguments, so nothing here needs a provider to find a
// peer — the member order below IS the dependency order (bus host, then the
// master over it, then the chip on the master).
//
// Note the two shapes of surface, and why the supply uses the second:
//
//   • GetLed() is a ROLE, declared on BoardProvider. Every board owes one,
//     binding MockLed when the hardware is absent. It stays a role because
//     "the status LED" is a thing application code addresses by meaning.
//
//   • GetPsu() is also a ROLE now. It was not, while there was one supply:
//     see BoardProvider.h for why a second one inverted that argument.
//
//   • GetDps() stays as a CONCRETE ACCESSOR, deliberately off BoardProvider —
//     the escape hatch for the day something needs a DPS5020's own API rather
//     than the role's. It is checked at compile time: a board without a DPS
//     simply has no GetDps(), and code calling it fails to build for that
//     board rather than at runtime. Nothing uses it today.
// ──────────────────────────────────────────────────────────────

class BoardContext : public BoardProvider
{
    static constexpr const char *TAG = "Board";

public:
    BoardContext() = default;

    BoardContext(const BoardContext &) = delete;
    BoardContext &operator=(const BoardContext &) = delete;
    BoardContext(BoardContext &&) = delete;
    BoardContext &operator=(BoardContext &&) = delete;

    void Init();

    // ── Roles (BoardProvider) ──
    Led &GetLed() override { return led_; }
    Psu &GetPsu() override { return dps_; }

    // ── Concrete driver accessors (off BoardProvider — see the note above) ──
    DPS5020 &GetDps() { return dps_; }

private:
    InitState initState_;

    // Hardware instances — buses first, then the drivers that use them.
    GpioLed led_{ BoardConfig::LED_PIN, BoardConfig::LED_ACTIVE_HIGH };

    ModbusRtuClient rtu_;
    ModbusMaster    master_{ rtu_ };
    DPS5020         dps_{ master_, BoardConfig::DPS_UNIT_ADDRESS };
};
