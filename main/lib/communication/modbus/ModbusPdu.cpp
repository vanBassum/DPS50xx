#include "ModbusPdu.h"
#include <cstring>

size_t ModbusPdu::Serialize(uint8_t *buffer, size_t bufferSize) const
{
    if (bufferSize < 1 + length)
    {
        return 0; // insufficient buffer
    }

    buffer[0] = functionCode;
    if (length > 0)
    {
        std::memcpy(&buffer[1], data, length);
    }

    return 1 + length; // total size written
}

ModbusPdu ModbusPdu::Deserialize(const uint8_t *buffer, size_t bufferSize)
{
    ModbusPdu pdu{};
    if (bufferSize == 0)
    {
        return pdu; // empty PDU
    }

    pdu.functionCode = buffer[0];
    if (bufferSize > 1)
    {
        size_t dataLen = bufferSize - 1;
        if (dataLen > MaxDataSize)
        {
            dataLen = MaxDataSize; // truncate safely
        }
        std::memcpy(pdu.data, &buffer[1], dataLen);
        pdu.length = dataLen;
    }
    return pdu;
}

// ----------------------
// Factory helpers
// ----------------------

ModbusPdu ModbusPdu::ReadCoils(uint16_t coilAddress, uint16_t quantity)
{
    ModbusPdu pdu{};
    pdu.functionCode = 0x01; // Read Coils

    pdu.data[0] = (coilAddress >> 8) & 0xFF;
    pdu.data[1] = coilAddress & 0xFF;
    pdu.data[2] = (quantity >> 8) & 0xFF;
    pdu.data[3] = quantity & 0xFF;

    pdu.length = 4;
    return pdu;
}

ModbusPdu ModbusPdu::ReadDiscreteInputs(uint16_t coilAddress, uint16_t quantity)
{
    ModbusPdu pdu{};
    pdu.functionCode = 0x02; // Read Discrete Inputs

    pdu.data[0] = (coilAddress >> 8) & 0xFF;
    pdu.data[1] = coilAddress & 0xFF;
    pdu.data[2] = (quantity >> 8) & 0xFF;
    pdu.data[3] = quantity & 0xFF;

    pdu.length = 4;
    return pdu;
}

ModbusPdu ModbusPdu::ReadHoldingRegisters(uint16_t registerAddress, uint16_t quantity)
{
    ModbusPdu pdu{};
    pdu.functionCode = 0x03; // Read Holding Registers

    pdu.data[0] = (registerAddress >> 8) & 0xFF;
    pdu.data[1] = registerAddress & 0xFF;
    pdu.data[2] = (quantity >> 8) & 0xFF;
    pdu.data[3] = quantity & 0xFF;

    pdu.length = 4;
    return pdu;
}

ModbusPdu ModbusPdu::ReadInputRegisters(uint16_t registerAddress, uint16_t quantity)
{
    ModbusPdu pdu{};
    pdu.functionCode = 0x04; // Read Input Registers

    pdu.data[0] = (registerAddress >> 8) & 0xFF;
    pdu.data[1] = registerAddress & 0xFF;
    pdu.data[2] = (quantity >> 8) & 0xFF;
    pdu.data[3] = quantity & 0xFF;

    pdu.length = 4;
    return pdu;
}

ModbusPdu ModbusPdu::WriteSingleCoil(uint16_t coilAddress, bool value)
{
    ModbusPdu pdu{};
    pdu.functionCode = 0x05; // Write Single Coil

    pdu.data[0] = (coilAddress >> 8) & 0xFF;
    pdu.data[1] = coilAddress & 0xFF;
    pdu.data[2] = value ? 0xFF : 0x00;
    pdu.data[3] = 0x00;

    pdu.length = 4;
    return pdu;
}

ModbusPdu ModbusPdu::WriteSingleRegister(uint16_t registerAddress, uint16_t value)
{
    ModbusPdu pdu{};
    pdu.functionCode = 0x06; // Write Single Register

    pdu.data[0] = (registerAddress >> 8) & 0xFF;
    pdu.data[1] = registerAddress & 0xFF;
    pdu.data[2] = (value >> 8) & 0xFF;
    pdu.data[3] = value & 0xFF;

    pdu.length = 4;
    return pdu;
}
