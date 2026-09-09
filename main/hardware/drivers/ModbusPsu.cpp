#include "ModbusPsu.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

void ModbusPsu::LinkRegister(ModbusReg& reg)
{
    reg.next = nullptr;
    if (regTail_ == nullptr)
        regHead_ = regTail_ = &reg;
    else
    {
        regTail_->next = &reg;
        regTail_ = &reg;
    }
}

ModbusError ModbusPsu::Poll()
{
    if (regHead_ == nullptr)
        return ModbusError::NotInitialized;   // a driver that registered nothing

    uint16_t buf[MAX_RUN];
    ModbusError err = ModbusError::NoError;
    bool optionalFailed = false;

    // The walk is over REGISTERS, not capabilities: this layer reads addresses
    // and knows nothing about what any of them mean.
    ModbusReg* reg = regHead_;
    while (reg != nullptr)
    {
        // Grow a run while the next register continues where the last one ended
        // AND shares its essentialness — a block that may fail without
        // condemning the supply cannot ride in the same transaction as one that
        // may not. Declaration order is address order in practice, so a
        // contiguous map yields exactly one transaction; a table with gaps costs
        // one per block, and one out of order merely costs an extra read rather
        // than reading the wrong thing.
        const uint16_t start = reg->address;
        const bool essential = reg->essential;
        uint16_t count = reg->words;

        ModbusReg* runEnd = reg->next;
        while (runEnd != nullptr &&
               runEnd->address == static_cast<uint16_t>(start + count) &&
               runEnd->essential == essential &&
               count + runEnd->words <= MAX_RUN)
        {
            count += runEnd->words;
            runEnd = runEnd->next;
        }

        const ModbusError runErr = ReadRun(start, count, buf, essential);
        if (runErr != ModbusError::NoError)
        {
            for (ModbusReg* r = reg; r != runEnd; r = r->next)
            {
                r->answered = false;
                if (r->target != nullptr)
                    r->target->SetUnavailable();
            }

            if (essential)
            {
                // The poll itself is a failure, which is what "offline" counts.
                err = runErr;
                break;
            }

            // Optional block: say so in the capabilities and carry on.
            optionalFailed = true;
            reg = runEnd;
            continue;
        }

        for (ModbusReg* r = reg; r != runEnd; r = r->next)
        {
            const uint16_t i = static_cast<uint16_t>(r->address - start);
            uint32_t raw = buf[i];
            if (r->words == 2)
            {
                const uint32_t other = buf[i + 1];
                raw = (r->order == PsuWordOrder::LowFirst)
                          ? (other << 16) | raw
                          : (raw << 16) | other;
            }
            r->raw = raw;
            r->answered = true;

            // Hand the value to the capability behind this register, if there
            // is one. A register with no target was read for the driver's own
            // arithmetic and is published nowhere.
            if (r->target != nullptr)
            {
                if (r->IsSentinel())
                    r->target->SetUnavailable();   // "nothing here", not a value
                else
                    r->target->Set(r->Value());
            }
        }

        reg = runEnd;
    }

    if (optionalFailed != optionalFailed_)
    {
        if (optionalFailed)
            ESP_LOGW(tag_, "an optional register block is not answering — the "
                           "capabilities behind it report unavailable, the supply "
                           "is unaffected");
        else
            ESP_LOGI(tag_, "optional register block answering again");
        optionalFailed_ = optionalFailed;
    }

    if (err != ModbusError::NoError)
    {
        failCount_++;
        if (online_ && failCount_ >= OFFLINE_THRESHOLD)
        {
            ESP_LOGW(tag_, "offline after %d consecutive failures (last: %s)",
                     failCount_, ModbusErrorToString(err));
            online_ = false;
        }
        return err;
    }

    if (!online_)
        ESP_LOGI(tag_, "online (addr=%d)", address_);
    online_ = true;
    failCount_ = 0;

    // Only now, with every register of this poll in place, does the driver get
    // to turn its protocol's shape into the capabilities it publishes.
    Normalize();
    return ModbusError::NoError;
}

ModbusError ModbusPsu::ReadRun(uint16_t start, uint16_t count, uint16_t* out, bool essential)
{
    ModbusError err = ModbusError::Timeout;

    // The supply's MCU also drives its display and keypad, so it misses requests
    // under load — see reasoning/2026-08-11-22h33 for what that looks like on the
    // wire when it is the supply and not the firmware.
    for (int attempt = 0; attempt < MAX_ATTEMPTS; ++attempt)
    {
        err = master_.ReadHoldingRegisters(address_, start, count, out, TIMEOUT_MS);
        if (err == ModbusError::NoError)
            return err;

        // An optional block that is simply absent would otherwise print this
        // every poll forever; Poll() reports its state changes instead.
        if (essential)
            ESP_LOGW(tag_, "read 0x%04X+%u attempt %d/%d failed: %s",
                     start, count, attempt + 1, MAX_ATTEMPTS, ModbusErrorToString(err));
        else
            ESP_LOGD(tag_, "optional read 0x%04X+%u failed: %s",
                     start, count, ModbusErrorToString(err));

        if (attempt + 1 < MAX_ATTEMPTS)
            vTaskDelay(pdMS_TO_TICKS(50));
    }
    return err;
}

ModbusReg* ModbusPsu::RegisterFor(const PsuCapability& capability) const
{
    // Which register is behind a capability is this layer's private business, so
    // it is answered by a walk rather than by a downcast or a pointer the
    // capability carries. Both would put transport back in the capability.
    for (ModbusReg* r = regHead_; r != nullptr; r = r->next)
        if (r->target == &capability)
            return r;
    return nullptr;
}

ModbusError ModbusPsu::Write(PsuCapability& capability, float value)
{
    if (!capability.Writable())
        return ModbusError::WritingNotAllowed;

    ModbusReg* reg = RegisterFor(capability);
    if (reg == nullptr)
        return ModbusError::NotImplemented;   // derived: no single register to write
    if (reg->words != 1)
        return ModbusError::NotImplemented;   // no multi-register write exists yet
    if (reg->scale <= 0.0f)
        return ModbusError::InvalidArguments;

    // Holding registers here are unsigned, so a negative setpoint is refused
    // rather than wrapped. This range is the REGISTER's; validating the
    // capability's own min/max is the command handler's half.
    const float raw = value / reg->scale + 0.5f;
    if (raw < 0.0f || raw > 65535.0f)
        return ModbusError::IllegalDataValue;

    const uint16_t word = static_cast<uint16_t>(raw);
    const ModbusError err = master_.WriteHoldingRegister(address_, reg->address, word, TIMEOUT_MS);

    // Store what the SUPPLY now holds (the rounded register), not what was
    // asked for, so an echoed reply cannot claim a precision the wire lost.
    if (err == ModbusError::NoError)
    {
        reg->raw = word;
        reg->answered = true;
        capability.Set(reg->Value());
        Normalize();   // a write can change what a derived capability means
    }
    return err;
}
