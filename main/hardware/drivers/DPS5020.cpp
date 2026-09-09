#include "DPS5020.h"

DPS5020::DPS5020(ModbusMaster& master, uint8_t address)
    : ModbusPsu(master, address, "DPS5020")
{
    // The whole driver. Order here is the order everything above sees: the
    // poll's batching, `psu get`, the telemetry point and the dashboard all
    // follow it.
    Add(setVoltage_);
    Add(setCurrent_);
    Add(outputVoltage_);
    Add(outputCurrent_);
    Add(outputPower_);
    Add(inputVoltage_);
    Add(keyLock_);
    Add(protectionState_);
    Add(constantCurrent_);
    Add(outputEnabled_);
    Add(backlight_);
    Add(model_);
    Add(version_);
}
