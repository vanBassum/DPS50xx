#include "XY6020L.h"

XY6020L::XY6020L(ModbusMaster& master, uint8_t address)
    : ModbusPsu(master, address, "XY6020L")
{
    // The whole driver. Compare DPS5020.cpp: a supply that reads nine more
    // things is nine more lines here and nothing anywhere else.
    Add(setVoltage_);
    Add(setCurrent_);
    Add(outVoltage_);
    Add(outCurrent_);
    Add(outPower_);
    Add(inVoltage_);
    Add(charge_);
    Add(energy_);
    Add(onHours_);
    Add(onMinutes_);
    Add(onSeconds_);
    Add(temperature_);
    Add(temperatureExt_);
    Add(keyLock_);
    Add(protection_);
    Add(constantCurrent_);
    Add(outputOn_);
    Add(model_);
    Add(version_);
    Add(memoryGroup_);
}
