#pragma once
#include <cstdint>
#include <cstddef>

struct ModbusPdu
{
    static constexpr size_t MaxDataSize = 252;
    static constexpr size_t MaxPduSize = 1 + MaxDataSize;

    uint8_t functionCode = 0;
    uint8_t data[MaxDataSize] = {0};
    size_t length = 0; // number of bytes in `data`

    // Serialize PDU into buffer
    size_t Serialize(uint8_t *buffer, size_t bufferSize) const;

    // Parse buffer into PDU
    static ModbusPdu Deserialize(const uint8_t *buffer, size_t bufferSize);

    // Factory helpers
    static ModbusPdu ReadCoils(uint16_t coilAddress, uint16_t quantity);
    static ModbusPdu ReadDiscreteInputs(uint16_t coilAddress, uint16_t quantity);
    static ModbusPdu ReadHoldingRegisters(uint16_t registerAddress, uint16_t quantity);
    static ModbusPdu ReadInputRegisters(uint16_t registerAddress, uint16_t quantity);
    static ModbusPdu WriteSingleCoil(uint16_t coilAddress, bool value);
    static ModbusPdu WriteSingleRegister(uint16_t registerAddress, uint16_t value);
};
