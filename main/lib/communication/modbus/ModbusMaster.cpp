#include "ModbusMaster.h"
#include "ModbusError.h"
#include "ModbusPdu.h"
#include "esp_log.h"

ModbusMaster::ModbusMaster(ModbusClient &client_)
    : client(client_)
{
}

ModbusError ModbusMaster::ReadCoil(uint8_t unitAddr, uint16_t coilAddress, bool *val, int timeoutMs)
{
    if (!val)
        return ModbusError::InvalidArguments;
    LOCK(mutex);

    ModbusPdu request = ModbusPdu::ReadCoils(coilAddress, 1);
    ModbusPdu response;

    ModbusError err = client.Execute(unitAddr, request, response, timeoutMs);
    if (err != ModbusError::NoError)
        return err;

    if (response.length < 2)
        return ModbusError::InvalidReplyLength;

    // response: function=0x01, byte count, coil status bytes
    *val = (response.data[1] & 0x01) != 0;
    return ModbusError::NoError;
}

ModbusError ModbusMaster::WriteCoil(uint8_t unitAddr, uint16_t coilAddress, bool val, int timeoutMs)
{
    LOCK(mutex);

    ModbusPdu request = ModbusPdu::WriteSingleCoil(coilAddress, val);
    ModbusPdu response;

    ModbusError err = client.Execute(unitAddr, request, response, timeoutMs);
    if (err != ModbusError::NoError)
        return err;

    if (response.length < 5)
        return ModbusError::InvalidReplyLength;

    uint16_t echoedAddr = (response.data[1] << 8) | response.data[2];
    uint16_t echoedVal  = (response.data[3] << 8) | response.data[4];
    uint16_t expectedVal = val ? 0xFF00 : 0x0000;

    if (echoedAddr != coilAddress || echoedVal != expectedVal)
        return ModbusError::InvalidDeviceReply_EchoMismatch;

    return ModbusError::NoError;
}


ModbusError ModbusMaster::ReadDiscreteInput(uint8_t unitAddr, uint16_t coilAddress, bool *val, int timeoutMs)
{
    if (!val)
        return ModbusError::InvalidArguments;
    LOCK(mutex);

    ModbusPdu request = ModbusPdu::ReadDiscreteInputs(coilAddress, 1);
    ModbusPdu response;

    ModbusError err = client.Execute(unitAddr, request, response, timeoutMs);
    if (err != ModbusError::NoError)
        return err;

    if (response.length < 2)
        return ModbusError::InvalidReplyLength;

    *val = (response.data[1] & 0x01) != 0;
    return ModbusError::NoError;
}

ModbusError ModbusMaster::ReadInputRegister(uint8_t unitAddr, uint16_t registerAddress, uint16_t *val, int timeoutMs)
{
    if (!val)
        return ModbusError::InvalidArguments;
    LOCK(mutex);

    ModbusPdu request = ModbusPdu::ReadInputRegisters(registerAddress, 1);
    ModbusPdu response;

    ModbusError err = client.Execute(unitAddr, request, response, timeoutMs);
    if (err != ModbusError::NoError)
        return err;

    if (response.length < 3)
        return ModbusError::InvalidReplyLength;

    // response: function=0x04, byte count, register values
    *val = (static_cast<uint16_t>(response.data[1]) << 8) | response.data[2];
    return ModbusError::NoError;
}

ModbusError ModbusMaster::ReadHoldingRegister(uint8_t unitAddr, uint16_t registerAddress, uint16_t *val, int timeoutMs)
{
    if (!val)
        return ModbusError::InvalidArguments;
    LOCK(mutex);

    ModbusPdu request = ModbusPdu::ReadHoldingRegisters(registerAddress, 1);
    ModbusPdu response;

    ModbusError err = client.Execute(unitAddr, request, response, timeoutMs);
    if (err != ModbusError::NoError)
        return err;

    if (response.length < 3)
        return ModbusError::InvalidReplyLength;

    *val = (static_cast<uint16_t>(response.data[1]) << 8) | response.data[2];
    return ModbusError::NoError;
}

ModbusError ModbusMaster::WriteHoldingRegister(uint8_t unitAddr, uint16_t registerAddress, uint16_t val, int timeoutMs)
{
    LOCK(mutex);

    ModbusPdu request = ModbusPdu::WriteSingleRegister(registerAddress, val);
    ModbusPdu response;
    ModbusError err = client.Execute(unitAddr, request, response, timeoutMs);
    if (err != ModbusError::NoError)
        return err;

    // Expected response: register + value = 4 bytes
    if (response.length < 4) {
        return ModbusError::InvalidReplyLength;
    }

    // Ensure the echoed address and value match what we sent
    uint16_t echoedAddr = (response.data[0] << 8) | response.data[1];
    uint16_t echoedVal  = (response.data[2] << 8) | response.data[3];

    if (echoedAddr != registerAddress || echoedVal != val) {
        return ModbusError::InvalidDeviceReply_EchoMismatch;
    }

    return ModbusError::NoError;
}

ModbusError ModbusMaster::ReadHoldingRegisters(uint8_t unitAddr, uint16_t startAddress, uint16_t quantity, uint16_t *vals, int timeoutMs)
{
    if (!vals || quantity == 0 || quantity > 125)
        return ModbusError::InvalidArguments;
    LOCK(mutex);

    ModbusPdu request = ModbusPdu::ReadHoldingRegisters(startAddress, quantity);
    ModbusPdu response;

    ModbusError err = client.Execute(unitAddr, request, response, timeoutMs);
    if (err != ModbusError::NoError)
        return err;

    // response.data[0] = byteCount, then register values in big-endian pairs
    uint8_t expectedBytes = quantity * 2;
    if (response.length < 1u + expectedBytes)
        return ModbusError::InvalidReplyLength;

    if (response.data[0] != expectedBytes)
        return ModbusError::InvalidReplyLength;

    for (uint16_t i = 0; i < quantity; i++)
    {
        vals[i] = (static_cast<uint16_t>(response.data[1 + i * 2]) << 8) |
                   response.data[2 + i * 2];
    }

    return ModbusError::NoError;
}
