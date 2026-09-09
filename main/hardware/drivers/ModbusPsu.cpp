#include "ModbusPsu.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// Every reading in a ModbusPsu's chain was Add()ed by a driver deriving from
// this class, and the only entries it can construct are PsuRegisters — so the
// downcasts below are safe by construction rather than by check. That is the
// same bargain CommandEntry's trampoline makes.

ModbusError ModbusPsu::Poll()
{
    const PsuReadingIterator stop = end();
    PsuReadingIterator it = begin();

    if (it == stop)
        return ModbusError::NotInitialized;   // a driver that registered nothing

    uint16_t buf[MAX_RUN];
    ModbusError err = ModbusError::NoError;

    while (it != stop)
    {
        // Grow a run for as long as the next register continues where the last
        // one ended. Declaration order is address order in practice, so a
        // contiguous map like the DPS5020's yields exactly one transaction; a
        // table with gaps costs one per block, and one out of order merely
        // costs an extra read rather than reading the wrong thing.
        const PsuRegister& first = static_cast<const PsuRegister&>(*it);
        const uint16_t start = first.address;
        uint16_t count = first.words;

        PsuReadingIterator runEnd = it;
        ++runEnd;
        while (runEnd != stop)
        {
            const PsuRegister& r = static_cast<const PsuRegister&>(*runEnd);
            if (r.address != static_cast<uint16_t>(start + count)) break;
            if (count + r.words > MAX_RUN) break;
            count += r.words;
            ++runEnd;
        }

        err = ReadRun(start, count, buf);
        if (err != ModbusError::NoError)
            break;      // readings from earlier runs keep the values just read

        for (PsuReadingIterator p = it; p != runEnd; ++p)
        {
            PsuRegister& r = static_cast<PsuRegister&>(*p);
            const uint16_t i = static_cast<uint16_t>(r.address - start);
            uint32_t raw = buf[i];
            if (r.words == 2)
                raw = (raw << 16) | buf[i + 1];   // high word first
            r.value = static_cast<float>(raw) * r.scale;
        }

        it = runEnd;
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
    return ModbusError::NoError;
}

ModbusError ModbusPsu::ReadRun(uint16_t start, uint16_t count, uint16_t* out)
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

        ESP_LOGW(tag_, "read 0x%04X+%u attempt %d/%d failed: %s",
                 start, count, attempt + 1, MAX_ATTEMPTS, ModbusErrorToString(err));
        if (attempt + 1 < MAX_ATTEMPTS)
            vTaskDelay(pdMS_TO_TICKS(50));
    }
    return err;
}

ModbusError ModbusPsu::Write(PsuReading& reading, float value)
{
    PsuRegister& r = static_cast<PsuRegister&>(reading);

    if (!r.Writable())
        return ModbusError::WritingNotAllowed;
    if (r.words != 1)
        return ModbusError::NotImplemented;   // no 32-bit setpoint exists yet
    if (r.scale <= 0.0f)
        return ModbusError::InvalidArguments;

    // Holding registers here are unsigned, so a negative setpoint is refused
    // rather than wrapped. Range is the register's, not the supply's — MEANING
    // validation against min/max is the command handler's half.
    const float raw = value / r.scale + 0.5f;
    if (raw < 0.0f || raw > 65535.0f)
        return ModbusError::IllegalDataValue;

    const uint16_t word = static_cast<uint16_t>(raw);
    const ModbusError err = master_.WriteHoldingRegister(address_, r.address, word, TIMEOUT_MS);

    // Store what the SUPPLY now holds (the rounded register), not what was
    // asked for, so an echoed reply cannot claim a precision the wire lost.
    if (err == ModbusError::NoError)
        r.value = static_cast<float>(word) * r.scale;
    return err;
}
