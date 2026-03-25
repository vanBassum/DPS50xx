#pragma once
#include "ModbusMaster.h"
#include "ModbusError.h"
#include <cstdint>

// DPS5020 Modbus holding register map
namespace DPS5020Reg
{
    constexpr uint16_t SetVoltage   = 0x0000; // R/W  0.01V
    constexpr uint16_t SetCurrent   = 0x0001; // R/W  0.01A
    constexpr uint16_t OutVoltage   = 0x0002; // R    0.01V
    constexpr uint16_t OutCurrent   = 0x0003; // R    0.01A
    constexpr uint16_t OutPower     = 0x0004; // R    0.01W
    constexpr uint16_t InVoltage    = 0x0005; // R    0.01V
    constexpr uint16_t KeyLock      = 0x0006; // R/W  0=unlocked, 1=locked
    constexpr uint16_t Protection   = 0x0007; // R    0=none, 1=OVP, 2=OCP, 3=OPP
    constexpr uint16_t CvCc         = 0x0008; // R    0=CV, 1=CC
    constexpr uint16_t OnOff        = 0x0009; // R/W  0=off, 1=on
    constexpr uint16_t Backlight    = 0x000A; // R/W  0-5
    constexpr uint16_t Model        = 0x000B; // R    e.g. 5020
    constexpr uint16_t Version      = 0x000C; // R    firmware version

    constexpr uint16_t RegStart     = 0x0000;
    constexpr uint16_t RegCount     = 13;     // 0x0000..0x000C
}

enum class DPS5020Protection : uint8_t
{
    None = 0,
    OVP  = 1, // Over Voltage Protection
    OCP  = 2, // Over Current Protection
    OPP  = 3, // Over Power Protection
};

struct DPS5020Data
{
    float setVoltage    = 0; // V
    float setCurrent    = 0; // A
    float outVoltage    = 0; // V
    float outCurrent    = 0; // A
    float outPower      = 0; // W
    float inVoltage     = 0; // V
    bool  keyLock       = false;
    DPS5020Protection protection = DPS5020Protection::None;
    bool  constantCurrent = false; // false=CV, true=CC
    bool  outputOn      = false;
    uint8_t backlight   = 0;
    uint16_t model      = 0;
    uint16_t version    = 0;
};

class DPS5020
{
    inline constexpr static const char* TAG = "DPS5020";
    static constexpr int TIMEOUT_MS = 300;

public:
    explicit DPS5020(ModbusMaster &master, uint8_t address = 1);

    // Read all registers in one batch
    ModbusError Poll();

    const DPS5020Data &GetData() const { return data_; }
    bool IsOnline() const { return online_; }

    // Control
    ModbusError SetVoltage(float volts);
    ModbusError SetCurrent(float amps);
    ModbusError SetOutput(bool on);
    ModbusError SetKeyLock(bool locked);
    ModbusError SetBacklight(uint8_t level);

private:
    ModbusMaster &master_;
    uint8_t address_;
    DPS5020Data data_{};
    bool online_ = false;
};
