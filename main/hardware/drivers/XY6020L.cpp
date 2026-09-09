#include "XY6020L.h"

XY6020L::XY6020L(ModbusMaster& master, uint8_t address)
    : ModbusPsu(master, address, "XY6020L")
{
    // Registration order is address order, which is what Poll() coalesces:
    // 0x0000–0x0012, then 0x0016–0x0017, then 0x001D, then the non-essential
    // 0x0052–0x005D block. Four transactions; the derived capabilities at the
    // end cost none, because they have no storage to read.
    //
    // Note the three bare ModbusRegs in the middle of the capability list. That
    // is the register/capability split doing its job: they take part in the
    // poll, they take part in nothing else.
    Add(setVoltage_);
    Add(setCurrent_);
    Add(outputVoltage_);
    Add(outputCurrent_);
    Add(outputPower_);
    Add(inputVoltage_);
    Add(chargeAh_);
    Add(energyWh_);
    Add(onHours_);
    Add(onMinutes_);
    Add(onSeconds_);
    Add(temperature_);
    Add(externalTemperature_);
    Add(keyLock_);
    Add(protectionState_);
    Add(constantCurrent_);
    Add(outputEnabled_);
    Add(model_);
    Add(version_);
    Add(activePreset_);

    Add(lvp_);
    Add(ovp_);
    Add(ocp_);
    Add(opp_);
    Add(ohpHours_);
    Add(ohpMinutes_);
    Add(oahLow_);
    Add(oahHigh_);
    Add(owhLow_);
    Add(owhHigh_);
    Add(otp_);
    Add(ini_);

    Add(runtimeSeconds_);
    Add(maxRuntime_);
    Add(maxCharge_);
    Add(maxEnergy_);
}

namespace
{
    /// Two registers holding one 32-bit value, low word first.
    uint64_t Combine32(const ModbusReg& low, const ModbusReg& high)
    {
        return (static_cast<uint64_t>(high.raw) << 16) | static_cast<uint64_t>(low.raw);
    }
}

void XY6020L::Normalize()
{
    // Runtime: three registers in, one whole number of seconds out. Accumulated
    // in uint64_t so the arithmetic cannot overflow whatever the panel has
    // counted, then held in the capability's double, which is exact to 2^53.
    const uint64_t seconds = static_cast<uint64_t>(onHours_.raw) * 3600ull +
                             static_cast<uint64_t>(onMinutes_.raw) * 60ull +
                             static_cast<uint64_t>(onSeconds_.raw);
    runtimeSeconds_.Set(static_cast<double>(seconds));

    // The configured limits, out of the block that may not have answered. Each
    // derived capability is available only if the registers behind it are —
    // which is how "unavailable" survives normalization instead of turning into
    // a confident zero.
    if (ohpHours_.answered && ohpMinutes_.answered)
        maxRuntime_.Set(static_cast<double>(
            static_cast<uint64_t>(ohpHours_.raw) * 3600ull +
            static_cast<uint64_t>(ohpMinutes_.raw) * 60ull));
    else
        maxRuntime_.SetUnavailable();

    if (oahLow_.answered && oahHigh_.answered)
        maxCharge_.Set(static_cast<double>(Combine32(oahLow_, oahHigh_)) * 0.001);
    else
        maxCharge_.SetUnavailable();

    if (owhLow_.answered && owhHigh_.answered)
        maxEnergy_.Set(static_cast<double>(Combine32(owhLow_, owhHigh_)) * 0.001);
    else
        maxEnergy_.SetUnavailable();
}
