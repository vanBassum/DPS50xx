#pragma once

#include "interfaces/Psu.h"

// ──────────────────────────────────────────────────────────────
// Psu role with no supply behind it: an empty capability chain, permanently
// offline. For a board with nothing on the Modbus wire.
//
// Unused by either board today, exactly like MockLed — it exists so that adding
// GetPsu() to BoardProvider does not oblige a future supply-less board to
// invent one. Note what falls out of an empty capability chain: `psu get`
// returns nothing, every write refuses with IllegalDataAddress, and the
// telemetry walk records nothing. None of that needed a special case.
// ──────────────────────────────────────────────────────────────
class MockPsu : public Psu
{
public:
    ModbusError Poll() override { return ModbusError::Disconnected; }
    bool IsOnline() const override { return false; }

    ModbusError Write(PsuCapability&, float) override { return ModbusError::Disconnected; }
    using Psu::Write;
};
