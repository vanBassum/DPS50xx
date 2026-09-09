#pragma once

#include "interfaces/Psu.h"

// ──────────────────────────────────────────────────────────────
// Psu role with no supply behind it: an empty register chain, permanently
// offline. For a board with nothing on the Modbus wire.
//
// Unused by either board today, exactly like MockLed — it exists so that adding
// GetPsu() to BoardProvider does not oblige a future supply-less board to
// invent one. Note what falls out of the chain being empty: `psu get` returns no
// readings, `psu set` refuses every key with IllegalDataAddress, and the
// telemetry walk records nothing. None of that needed a special case.
// ──────────────────────────────────────────────────────────────
class MockPsu : public Psu
{
public:
    ModbusError Poll() override { return ModbusError::Disconnected; }
    bool IsOnline() const override { return false; }

    ModbusError Write(PsuReading&, float) override { return ModbusError::Disconnected; }
    using Psu::Write;
};
