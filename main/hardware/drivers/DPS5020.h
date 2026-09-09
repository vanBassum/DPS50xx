#pragma once

#include "ModbusPsu.h"

// ──────────────────────────────────────────────────────────────
// DPS5020 bench supply: the register map, and nothing else.
//
// Everything that used to be here — the batch read, the retry loop, the offline
// threshold, the decode, the setters — is in ModbusPsu, and the application
// reads this supply through the capabilities below rather than by naming its
// fields. So a second supply is a second table, not a second copy of any of it.
//
// This map needs no normalization at all: every register already means what a
// capability means, so there is no Normalize() override here. Compare XY6020L,
// where three registers hold one runtime and a probe reports 8888 for "absent" —
// that is what the capability layer is for, and a supply that does not need it
// pays nothing.
//
// Register map (holding registers, 0.01 scaling on the analogue ones):
//   0x0000 set voltage   R/W      0x0007 protection    R
//   0x0001 set current   R/W      0x0008 CV/CC         R
//   0x0002 out voltage   R        0x0009 output on     R/W
//   0x0003 out current   R        0x000A backlight     R/W
//   0x0004 out power     R        0x000B model         R
//   0x0005 in voltage    R        0x000C version       R
//   0x0006 key lock      R/W
//
// The map is contiguous, so ModbusPsu::Poll() coalesces the whole table into
// one transaction — the same single round trip the hand-written driver made.
// ──────────────────────────────────────────────────────────────
class DPS5020 : public ModbusPsu
{
public:
    explicit DPS5020(ModbusMaster& master, uint8_t address = 1);

private:
    /// Indexed by the protection register's own code. Naming the codes is the
    /// supply's job — a supply with more states needs no agreement from the
    /// application or the frontend.
    static constexpr const char* const kProtection[] = { "None", "OVP", "OCP", "OPP" };

    // ── The table. Registration order is register order is display order.
    //
    // Telemetry names are the trailing column and they are DELIBERATELY not the
    // capability keys: `voltage` and `inputVoltage` are what the existing Influx
    // series are called, and renaming one silently orphans its history. A
    // capability with no telemetry name is simply not recorded.
    //
    // min/max on the setpoints are this supply's range: what the handler
    // validates against and what the browser draws its input bounds from.
    Setpoint    setVoltage_    { "setVoltage", "Set Voltage", "V", Reg{0x0000, 0.01f}, 0.0f, 50.0f, "setVoltage" };
    Setpoint    setCurrent_    { "setCurrent", "Set Current", "A", Reg{0x0001, 0.01f}, 0.0f, 20.0f, "setCurrent" };
    Measurement outputVoltage_ { "outputVoltage", "Output Voltage", "V", Reg{0x0002, 0.01f}, "voltage" };
    Measurement outputCurrent_ { "outputCurrent", "Output Current", "A", Reg{0x0003, 0.01f}, "current" };
    Measurement outputPower_   { "outputPower", "Output Power", "W", Reg{0x0004, 0.01f}, "power" };
    Measurement inputVoltage_  { "inputVoltage", "Input Voltage", "V", Reg{0x0005, 0.01f}, "inputVoltage" };
    Switch      keyLock_       { "keyLock", "Key Lock", Reg{0x0006} };
    Selector    protectionState_{ "protectionState", "Protection", Reg{0x0007}, kProtection };
    StateFlag   constantCurrent_ { "constantCurrent", "Constant Current", Reg{0x0008} };
    Switch      outputEnabled_ { "outputEnabled", "Output", Reg{0x0009}, "output" };
    Setpoint    backlight_     { "backlight", "Backlight", "", Reg{0x000A}, 0.0f, 5.0f };
    DeviceInfo  model_         { "model", "Model", Reg{0x000B} };
    DeviceInfo  version_       { "version", "Firmware", Reg{0x000C} };
};
