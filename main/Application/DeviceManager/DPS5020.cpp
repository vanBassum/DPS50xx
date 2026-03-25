#include "DPS5020.h"
#include "esp_log.h"

DPS5020::DPS5020(ModbusMaster &master, uint8_t address)
    : master_(master), address_(address)
{
}

ModbusError DPS5020::Poll()
{
    uint16_t regs[DPS5020Reg::RegCount];
    ModbusError err = ModbusError::Timeout;

    // Retry up to MAX_RETRIES times - the DPS5020 MCU is shared with
    // display/controls and frequently misses Modbus requests
    for (int attempt = 0; attempt < MAX_RETRIES; attempt++)
    {
        err = master_.ReadHoldingRegisters(
            address_, DPS5020Reg::RegStart, DPS5020Reg::RegCount, regs, TIMEOUT_MS);

        if (err == ModbusError::NoError)
            break;

        ESP_LOGD(TAG, "Attempt %d/%d failed: %s", attempt + 1, MAX_RETRIES, ModbusErrorToString(err));
        vTaskDelay(pdMS_TO_TICKS(50));
    }

    if (err != ModbusError::NoError)
    {
        failCount_++;
        if (online_ && failCount_ >= OFFLINE_THRESHOLD)
        {
            ESP_LOGW(TAG, "DPS5020 offline after %d consecutive failures (last: %s)",
                     failCount_, ModbusErrorToString(err));
            online_ = false;
        }
        return err;
    }

    if (!online_)
    {
        ESP_LOGI(TAG, "DPS5020 online (addr=%d)", address_);
    }
    online_ = true;
    failCount_ = 0;

    data_.setVoltage      = regs[DPS5020Reg::SetVoltage]  / 100.0f;
    data_.setCurrent      = regs[DPS5020Reg::SetCurrent]  / 100.0f;
    data_.outVoltage      = regs[DPS5020Reg::OutVoltage]  / 100.0f;
    data_.outCurrent      = regs[DPS5020Reg::OutCurrent]  / 100.0f;
    data_.outPower        = regs[DPS5020Reg::OutPower]    / 100.0f;
    data_.inVoltage       = regs[DPS5020Reg::InVoltage]   / 100.0f;
    data_.keyLock         = regs[DPS5020Reg::KeyLock] != 0;
    data_.protection      = static_cast<DPS5020Protection>(regs[DPS5020Reg::Protection] & 0xFF);
    data_.constantCurrent = regs[DPS5020Reg::CvCc] != 0;
    data_.outputOn        = regs[DPS5020Reg::OnOff] != 0;
    data_.backlight       = regs[DPS5020Reg::Backlight] & 0xFF;
    data_.model           = regs[DPS5020Reg::Model];
    data_.version         = regs[DPS5020Reg::Version];

    return ModbusError::NoError;
}

ModbusError DPS5020::SetVoltage(float volts)
{
    uint16_t raw = static_cast<uint16_t>(volts * 100.0f + 0.5f);
    ModbusError err = master_.WriteHoldingRegister(address_, DPS5020Reg::SetVoltage, raw, TIMEOUT_MS);
    if (err == ModbusError::NoError)
        data_.setVoltage = raw / 100.0f;
    return err;
}

ModbusError DPS5020::SetCurrent(float amps)
{
    uint16_t raw = static_cast<uint16_t>(amps * 100.0f + 0.5f);
    ModbusError err = master_.WriteHoldingRegister(address_, DPS5020Reg::SetCurrent, raw, TIMEOUT_MS);
    if (err == ModbusError::NoError)
        data_.setCurrent = raw / 100.0f;
    return err;
}

ModbusError DPS5020::SetOutput(bool on)
{
    ModbusError err = master_.WriteHoldingRegister(address_, DPS5020Reg::OnOff, on ? 1 : 0, TIMEOUT_MS);
    if (err == ModbusError::NoError)
        data_.outputOn = on;
    return err;
}

ModbusError DPS5020::SetKeyLock(bool locked)
{
    ModbusError err = master_.WriteHoldingRegister(address_, DPS5020Reg::KeyLock, locked ? 1 : 0, TIMEOUT_MS);
    if (err == ModbusError::NoError)
        data_.keyLock = locked;
    return err;
}

ModbusError DPS5020::SetBacklight(uint8_t level)
{
    if (level > 5)
        level = 5;
    ModbusError err = master_.WriteHoldingRegister(address_, DPS5020Reg::Backlight, level, TIMEOUT_MS);
    if (err == ModbusError::NoError)
        data_.backlight = level;
    return err;
}
