#pragma once

#include "ModbusPsu.h"

// ──────────────────────────────────────────────────────────────
// XY6020L (60 V / 20 A / 1200 W): the register map, and the normalization that
// turns it into capabilities.
//
// Read the two halves of the table below as two different things. The `Reg{…}`
// values are where this device happens to store data. The capability names are
// what the product offers — and three of them exist precisely BECAUSE the
// device's storage does not match the meaning:
//
//   • runtimeSeconds. The device keeps hours, minutes and seconds in three
//     registers. Those are `Reg` members with no capability behind them, so
//     they are polled and published nowhere; Normalize() folds them into one
//     semantic value. Nothing above this file learns there were three.
//   • externalTemperature. An unconnected probe answers 8888, which is not
//     888.8 °C. Declared as a sentinel here, so the capability reports
//     "unavailable" and the number never reaches a UI.
//   • activePreset. A discrete choice, not a measurement, so it is a Selector
//     carrying this panel's own M0–M9 names rather than a float that renders
//     as "0.00".
//
// BENCH-VERIFIED on 2026-09-09, including the memory-group block: every address
// below answered, the sentinel fired on an unplugged probe, and the protections
// read values a panel would plausibly hold (LVP 6.0 V, OVP 62.0 V, OCP 22.0 A,
// OTP 95 °C — the last being exactly the vendor's documented default, which is
// what confirms that block's 1-per-unit scales). The block stays `.Optional()`
// anyway: it is configuration, and a supply that stops answering it should
// report those capabilities unavailable rather than be declared offline.
//
// Still unverified: the live power scale (needs a load) and what `model` means.
// Sources: register indices from the Jens3382/xy6020l Arduino library (credited
// to g-radmac for reversing the protocol) plus the vendor's Modbus document.
//
// Live block, one transaction:
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
//   0x000C on time sec    R              0x001D memory group    R
//
// Memory group M0's block, where the CONFIGURED protections live (M0 at 0x0050,
// stride 0x10, so M1 is 0x0060). NOTE this reads M0's copy whatever preset is
// active — correct while the panel is on M0, which is what it reports today.
//   0x0050 group Vset  (not read)         0x0058 OAH low   0.001 Ah
//   0x0051 group Iset  (not read)         0x0059 OAH high
//   0x0052 LVP         0.01 V             0x005A OWH low   0.001 Wh
//   0x0053 OVP         0.01 V             0x005B OWH high
//   0x0054 OCP         0.01 A             0x005C OTP       1 °C
//   0x0055 OPP         0.1 W              0x005D INI       output on boot
//   0x0056 OHP hours                      (Ah, Wh and hours+minutes are
//   0x0057 OHP minutes                     read-only — see Normalize())
// ──────────────────────────────────────────────────────────────
class XY6020L : public ModbusPsu
{
public:
    explicit XY6020L(ModbusMaster& master, uint8_t address = 1);

protected:
    /// Where this device's storage stops being anyone else's problem.
    void Normalize() override;

private:
    /// Indexed by the protection register's own code. Eleven states where a
    /// DPS5020 has four, which is why naming them is the driver's job.
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

    /// The ten presets, named the way the front panel names them.
    static constexpr const char* const kPresets[] = {
        "M0", "M1", "M2", "M3", "M4", "M5", "M6", "M7", "M8", "M9",
    };

    /// An unconnected external probe reads 8888 raw. The one number in this
    /// file that a UI must never see.
    static constexpr int32_t PROBE_ABSENT_RAW = 8888;

    // ── Capabilities on the live block ──
    Setpoint    setVoltage_    { "setVoltage", "Set Voltage", "V", Reg{0x0000, 0.01f}, 0.0f, 60.0f, "setVoltage" };
    Setpoint    setCurrent_    { "setCurrent", "Set Current", "A", Reg{0x0001, 0.01f}, 0.0f, 20.0f, "setCurrent" };
    Measurement outputVoltage_ { "outputVoltage", "Output Voltage", "V", Reg{0x0002, 0.01f}, "voltage" };
    Measurement outputCurrent_ { "outputCurrent", "Output Current", "A", Reg{0x0003, 0.01f}, "current" };

    /// 0.1 W, not 0.01. The library this map came from contradicts itself — the
    /// register comment says 0,1 W, the accessor's doxygen says 0.01 W — and
    /// arithmetic decides it: a 1200 W supply cannot fit in 0.01 W steps of an
    /// unsigned 16-bit register (655.35 W). Unconfirmed under load; reading 10×
    /// low is the signature of it being wrong.
    Measurement outputPower_   { "outputPower", "Output Power", "W", Reg{0x0004, 0.1f}, "power" };
    Measurement inputVoltage_  { "inputVoltage", "Input Voltage", "V", Reg{0x0005, 0.01f}, "inputVoltage" };

    SessionValue chargeAh_ { "chargeAh", "Charge", "Ah",
                             Reg{0x0006, 0.001f}.Pair(PsuWordOrder::LowFirst), "charge" };
    SessionValue energyWh_ { "energyWh", "Energy", "Wh",
                             Reg{0x0008, 0.001f}.Pair(PsuWordOrder::LowFirst), "energy" };

    // Storage with no capability behind it: the output timer, as three
    // registers. Polled, published nowhere, folded into runtimeSeconds_.
    ModbusReg onHours_   { 0x000A };
    ModbusReg onMinutes_ { 0x000B };
    ModbusReg onSeconds_ { 0x000C };

    SessionValue temperature_ { "temperature", "Temperature", "°C",
                                Reg{0x000D, 0.1f}, "temperature" };
    SessionValue externalTemperature_ { "externalTemperature", "External Probe", "°C",
                                        Reg{0x000E, 0.1f}.Sentinel(PROBE_ABSENT_RAW),
                                        "temperatureExt" };

    Switch   keyLock_         { "keyLock", "Key Lock", Reg{0x000F} };
    Selector protectionState_ { "protectionState", "Protection", Reg{0x0010}, kProtection };
    StateFlag constantCurrent_ { "constantCurrent", "Constant Current", Reg{0x0011} };
    Switch   outputEnabled_   { "outputEnabled", "Output", Reg{0x0012}, "output" };

    // ── Identity: a second transaction ──
    // Both answer every poll; what they MEAN is unconfirmed (model reads
    // 25858), which is one more reason they are Info and on no dashboard.
    DeviceInfo model_   { "model", "Model", Reg{0x0016} };
    DeviceInfo version_ { "version", "Firmware", Reg{0x0017} };

    // ── The active preset: a third transaction ──
    // Read-only: recalling a preset rewrites every setpoint at once, and this
    // firmware has never done it. Reporting which one is active is honest;
    // offering to change it would be inventing a feature the backend does not have.
    Selector activePreset_ { "activePreset", "Preset", Reg{0x001D}, kPresets };

    // ── M0's configured protections: a fourth, non-essential transaction ──
    Limit lvp_ { "protLowVoltage",  "Low voltage",  "V", Reg{0x0052, 0.01f}.Optional(), 0.0f, 60.0f };
    Limit ovp_ { "protOverVoltage", "Over voltage", "V", Reg{0x0053, 0.01f}.Optional(), 0.0f, 65.0f };

    /// 25 A, not the supply's 20 A rating: this unit came back holding 22.0 A,
    /// so the rating is not this register's ceiling and refusing to re-enter a
    /// value the panel already accepted would be the same mistake MAX_CURRENT
    /// made on the DPS5020.
    Limit ocp_ { "protOverCurrent", "Over current", "A", Reg{0x0054, 0.01f}.Optional(), 0.0f, 25.0f };

    /// 1 W per count, where the LIVE power register (0x0004) is 0.1 W. Inferred,
    /// not measured: this unit reads raw 1200 here, which is 1200 W — exactly
    /// its rating — at 1 W and an odd 120 W at 0.1 W; and the vendor's
    /// documented default of 950 W would be an implausible 95 W at 0.1. The
    /// neighbouring OTP register confirms the block uses natural units where
    /// the live block uses hundredths.
    Limit opp_ { "protOverPower",   "Over power",   "W", Reg{0x0055, 1.0f}.Optional(),  0.0f, 1200.0f };

    // More storage without capabilities: max runtime is hours + minutes, and the
    // Ah/Wh limits are 32-bit pairs. All three become read-only derived
    // capabilities, because writing them needs a multi-register write that
    // ModbusPsu does not do and that these unverified addresses do not deserve.
    ModbusReg ohpHours_   { Reg{0x0056}.Optional() };
    ModbusReg ohpMinutes_ { Reg{0x0057}.Optional() };
    ModbusReg oahLow_     { Reg{0x0058}.Optional() };
    ModbusReg oahHigh_    { Reg{0x0059}.Optional() };
    ModbusReg owhLow_     { Reg{0x005A}.Optional() };
    ModbusReg owhHigh_    { Reg{0x005B}.Optional() };

    /// 1 °C per count, confirmed: this unit reads 95, which is the vendor's
    /// documented default of 95 °C.
    Limit       otp_ { "protMaxTemperature", "Max temperature", "°C", Reg{0x005C, 1.0f}.Optional(), 0.0f, 120.0f };
    LimitSwitch ini_ { "protOutputOnBoot",   "Output on boot",        Reg{0x005D}.Optional() };

    // ── Capabilities with no storage of their own ──
    DerivedCapability runtimeSeconds_ { "runtimeSeconds", "Runtime", "s",
                                        PsuKind::Duration, PsuGroup::Session, "runtime" };
    DerivedCapability maxRuntime_ { "protMaxRuntime", "Max runtime", "s",
                                    PsuKind::Duration, PsuGroup::Protection };
    DerivedCapability maxCharge_  { "protMaxCharge", "Max charge", "Ah",
                                    PsuKind::Number, PsuGroup::Protection };
    DerivedCapability maxEnergy_  { "protMaxEnergy", "Max energy", "Wh",
                                    PsuKind::Number, PsuGroup::Protection };

    // Absent by design, and needing no special case anywhere: this supply has no
    // backlight, so `psu set -backlight 3` is refused by the same handler that
    // accepts it on a DPS5020, and the dashboard draws no such control.
};
