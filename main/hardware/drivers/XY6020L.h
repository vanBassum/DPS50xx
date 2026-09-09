#pragma once

#include "ModbusPsu.h"

// ──────────────────────────────────────────────────────────────
// XY6020L (60 V / 20 A / 1200 W) bench supply: the register map, and nothing
// else. Second driver on the chain, and the reason the chain exists — it reads
// nine things a DPS5020 cannot, and none of them cost a line above this file.
//
// ⚠ THE WHOLE MAP IS BENCH-UNVERIFIED. It comes from the register indices in
// the Jens3382/xy6020l Arduino library (which credits g-radmac for reversing
// the UART protocol) plus the protection codes from the vendor's Modbus
// interface document. Nothing here has been compared against a real unit, so
// treat every address, scale and label as a claim to check rather than a fact —
// see docs/next-up.md for what to check first.
//
// Register map (holding registers):
//   0x0000 set voltage    R/W  0.01 V     0x000D internal temp   R    0.1 °C
//   0x0001 set current    R/W  0.01 A     0x000E external temp   R    0.1 °C
//   0x0002 out voltage    R    0.01 V     0x000F key lock        R/W
//   0x0003 out current    R    0.01 A     0x0010 protection      R
//   0x0004 out power      R    0.1 W      0x0011 CV/CC           R
//   0x0005 in voltage     R    0.01 V     0x0012 output on       R/W
//   0x0006 charge  LOW    R    0.001 Ah   0x0013 °C/°F unit      R/W  (not read)
//   0x0007 charge  HIGH   R              0x0016 model           R
//   0x0008 energy  LOW    R    0.001 Wh   0x0017 version         R
//   0x0009 energy  HIGH   R              0x0018 slave address   R/W  (not read)
//   0x000A on time h      R              0x0019 baud rate       W    (not read)
//   0x000B on time min    R              0x001A/1B temp offsets R/W  (not read)
//   0x000C on time sec    R              0x001D memory group    R/W
//                                         0x0050+ preset blocks (not read)
//
// Three things this map does that the DPS5020's did not, and each is a reason
// the phase-1 mechanism earned its keep:
//
//   • 32-bit values. Charge and energy span a register pair, LOW at the lower
//     address — hence PsuWordOrder::LowFirst, and hence there being no default
//     worth trusting.
//   • Gaps. The table skips 0x0013–0x0015 and everything past 0x0017, so
//     Poll() splits into three transactions instead of one. Registers a poll
//     has no business touching (the baud rate, the slave address) are simply
//     not in the table.
//   • A different power scale. 0.1 W, not the DPS's 0.01 W — see the note on
//     outPower_ below, because the sources disagree and arithmetic settles it.
// ──────────────────────────────────────────────────────────────
class XY6020L : public ModbusPsu
{
public:
    explicit XY6020L(ModbusMaster& master, uint8_t address = 1);

private:
    /// Indexed by the protection register's own code. Eleven states where a
    /// DPS5020 has four, which is exactly why naming them is the driver's job
    /// and not the frontend's.
    static constexpr const char* const kProtection[] = {
        "None",  // 0
        "OVP",   // 1  over voltage
        "OCP",   // 2  over current
        "OPP",   // 3  over power
        "LVP",   // 4  input under voltage
        "OAH",   // 5  capacity (Ah) limit reached
        "OHP",   // 6  run-time limit reached
        "OTP",   // 7  over temperature
        "OEP",   // 8  energy (Wh) limit reached
        "OWH",   // 9
        "ICP",   // 10 input current
    };

    // ── The table. Declaration order is register order is display order.
    //
    // Telemetry names deliberately match the DPS5020's where the concept is the
    // same (`voltage`, `current`, `power`, `inputVoltage`, `output`), so the two
    // supplies land in ONE Influx series and a dashboard query does not care
    // which was on the bench. The four this supply adds are new names.
    WriteNumber setVoltage_ { "setVoltage", "Set Voltage",    "V", 0x0000, 0.01f, 0.0f, 60.0f, "setVoltage" };
    WriteNumber setCurrent_ { "setCurrent", "Set Current",    "A", 0x0001, 0.01f, 0.0f, 20.0f, "setCurrent" };
    ReadNumber  outVoltage_ { "outVoltage", "Output Voltage", "V", 0x0002, 0.01f, "voltage" };
    ReadNumber  outCurrent_ { "outCurrent", "Output Current", "A", 0x0003, 0.01f, "current" };

    /// 0.1 W, not 0.01. The library this map came from contradicts itself — the
    /// register comment says 0,1 W and the accessor's doxygen says 0.01 W — and
    /// the arithmetic decides it: this is a 1200 W supply, and 0.01 W in an
    /// unsigned 16-bit register tops out at 655.35 W. A wrong guess here reads
    /// 10× low, which is obvious on the first bench reading.
    ReadNumber  outPower_   { "outPower",   "Output Power",   "W", 0x0004, 0.1f,  "power" };
    ReadNumber  inVoltage_  { "inVoltage",  "Input Voltage",  "V", 0x0005, 0.01f, "inputVoltage" };

    // The 32-bit accumulators. Two registers each, LOW word first.
    ReadNumber  charge_     { "charge", "Charge", "Ah", 0x0006, 0.001f, "charge", 2, PsuWordOrder::LowFirst };
    ReadNumber  energy_     { "energy", "Energy", "Wh", 0x0008, 0.001f, "energy", 2, PsuWordOrder::LowFirst };

    // Output timer, as the supply keeps it: three separate registers, not one
    // duration. Left as three readings rather than composed into seconds — the
    // chain reports what the supply has, and a dashboard can add them up.
    ReadNumber  onHours_    { "onHours",   "On Time (h)",   "h", 0x000A, 1.0f };
    ReadNumber  onMinutes_  { "onMinutes", "On Time (min)", "m", 0x000B, 1.0f };
    ReadNumber  onSeconds_  { "onSeconds", "On Time (s)",   "s", 0x000C, 1.0f };

    ReadNumber  temperature_    { "temperature",    "Temperature",   "°C", 0x000D, 0.1f, "temperature" };
    ReadNumber  temperatureExt_ { "temperatureExt", "External Probe", "°C", 0x000E, 0.1f, "temperatureExt" };

    WriteBool   keyLock_    { "keyLock",    "Key Lock",   0x000F };
    ReadEnum    protection_ { "protection", "Protection", 0x0010, kProtection };
    ReadBool    constantCurrent_ { "constantCurrent", "Constant Current", 0x0011 };
    WriteBool   outputOn_   { "outputOn",   "Output",     0x0012, "output" };

    // Gap: 0x0013 (°C/°F), 0x0014, 0x0015 are not read — the temperature unit
    // would change what the two scales above MEAN, so this driver leaves it
    // alone rather than reporting a number whose unit it does not control.
    ReadNumber  model_      { "model",   "Model",    "", 0x0016, 1.0f };
    ReadNumber  version_    { "version", "Firmware", "", 0x0017, 1.0f };

    /// Which of the ten front-panel presets is active. Read-only here: RECALLING
    /// one is a write with side effects on every setpoint at once, which belongs
    /// in the typed `psu adv` command the backlog describes, not in a generic
    /// key/value write.
    ReadNumber  memoryGroup_{ "memoryGroup", "Memory Group", "", 0x001D, 1.0f };

    // Note what is ABSENT and needs no special case anywhere: this supply has no
    // backlight register, so `psu set -backlight 3` comes back "this supply has
    // no 'backlight'" from the same handler that accepts it on a DPS5020, and
    // the dashboard simply has no such control to draw.
};
