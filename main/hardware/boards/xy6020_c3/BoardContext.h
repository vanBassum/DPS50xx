#pragma once

#include "InitState.h"
#include "BoardConfig.h"
#include "interfaces/BoardProvider.h"
#include "drivers/GpioLed.h"
#include "drivers/XY6020L.h"
#include "ModbusRtuClient.h"
#include "ModbusMaster.h"

// ──────────────────────────────────────────────────────────────
// The board layer's context for an ESP32-C3 SuperMini driving an XY6020L:
// owns every driver instance and the Modbus bus host, and answers
// BoardProvider.
//
// Byte-for-byte the shape of dps50xx_c3's, with one driver type and one set of
// constants changed. That is the whole cost of a second supply: the application
// layer, the framework and the frontend are untouched, because everything above
// this file reaches the supply as a Psu and reads what its table declared.
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
//   • GetPsu() is also a ROLE, and this board is why: two supplies is what
//     made the role the intersection of two maps rather than one chip's API
//     wearing a role's name. See BoardProvider.h.
//
//   • There is NO GetDps() here, and that is the concrete-accessor escape
//     hatch working as intended: application code that reached for a DPS5020's
//     own API would fail to build for this board instead of failing at
//     runtime. Nothing reaches for one, which is why this compiles.
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
    Psu &GetPsu() override { return psu_; }

private:
    InitState initState_;

    // Hardware instances — buses first, then the drivers that use them.
    GpioLed led_{ BoardConfig::LED_PIN, BoardConfig::LED_ACTIVE_HIGH };

    ModbusRtuClient rtu_;
    ModbusMaster    master_{ rtu_ };
    XY6020L         psu_{ master_, BoardConfig::PSU_UNIT_ADDRESS };
};
