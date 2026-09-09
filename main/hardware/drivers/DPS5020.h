#pragma once

#include "ModbusPsu.h"

// ──────────────────────────────────────────────────────────────
// DPS5020 bench supply: the register map, and nothing else.
//
// Everything that used to be here — the batch read, the retry loop, the offline
// threshold, the decode, the setters — is in ModbusPsu now, and the application
// reads this supply by walking the chain below rather than by naming its fields.
// So a second supply is a second table, not a second copy of any of that.
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
// one transaction — same single round trip the hand-written driver made.
// ──────────────────────────────────────────────────────────────
class DPS5020 : public ModbusPsu
{
public:
    explicit DPS5020(ModbusMaster& master, uint8_t address = 1);

private:
    /// Indexed by the protection register's own code. Naming the codes is the
    /// supply's job — a supply with more states than this one needs no agreement
    /// from the application or the frontend.
    static constexpr const char* const kProtection[] = { "None", "OVP", "OCP", "OPP" };

    // ── The table. Declaration order is register order is display order.
    //
    // Telemetry names are the third column and they are DELIBERATELY not the
    // keys: `voltage` and `inputVoltage` are what the existing Influx series are
    // called, and renaming one silently orphans its history. A reading with no
    // telemetry name is simply not recorded.
    //
    // min/max on the setpoints are this supply's range. They are what the
    // handler validates against and what the browser draws its input bounds
    // from — MAX_VOLTAGE and `max={50}` are both gone.
    WriteNumber setVoltage_ { "setVoltage", "Set Voltage",    "V", 0x0000, 0.01f, 0.0f, 50.0f, "setVoltage" };
    WriteNumber setCurrent_ { "setCurrent", "Set Current",    "A", 0x0001, 0.01f, 0.0f, 20.0f, "setCurrent" };
    ReadNumber  outVoltage_ { "outVoltage", "Output Voltage", "V", 0x0002, 0.01f, "voltage" };
    ReadNumber  outCurrent_ { "outCurrent", "Output Current", "A", 0x0003, 0.01f, "current" };
    ReadNumber  outPower_   { "outPower",   "Output Power",   "W", 0x0004, 0.01f, "power" };
    ReadNumber  inVoltage_  { "inVoltage",  "Input Voltage",  "V", 0x0005, 0.01f, "inputVoltage" };
    WriteBool   keyLock_    { "keyLock",    "Key Lock",            0x0006 };
    ReadEnum    protection_ { "protection", "Protection",          0x0007, kProtection };
    ReadBool    constantCurrent_ { "constantCurrent", "Constant Current", 0x0008 };
    WriteBool   outputOn_   { "outputOn",   "Output",              0x0009, "output" };
    WriteNumber backlight_  { "backlight",  "Backlight",      "",  0x000A, 1.0f, 0.0f, 5.0f };
    ReadNumber  model_      { "model",      "Model",          "",  0x000B, 1.0f };
    ReadNumber  version_    { "version",    "Firmware",       "",  0x000C, 1.0f };
};
