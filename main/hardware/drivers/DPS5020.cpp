#include "DPS5020.h"

DPS5020::DPS5020(ModbusMaster& master, uint8_t address)
    : ModbusPsu(master, address, "DPS5020")
{
    // The whole driver. Order here is the order everything above sees: the poll's
    // batching, `psu get`, the telemetry point and the dashboard all follow it.
    Add(setVoltage_);
    Add(setCurrent_);
    Add(outVoltage_);
    Add(outCurrent_);
    Add(outPower_);
    Add(inVoltage_);
    Add(keyLock_);
    Add(protection_);
    Add(constantCurrent_);
    Add(outputOn_);
    Add(backlight_);
    Add(model_);
    Add(version_);
}
