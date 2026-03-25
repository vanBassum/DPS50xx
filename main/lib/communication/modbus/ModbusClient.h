#pragma once
#include <cstdint>
#include "ModbusError.h"
#include "ModbusPdu.h"

class ModbusClient
{
public:
    virtual ~ModbusClient() = default;

    virtual bool Connect(int timeoutMs) = 0;
    virtual void Disconnect() = 0;
    // Execute a request PDU, return response PDU
    virtual ModbusError Execute(uint8_t unitId, const ModbusPdu &request, ModbusPdu &response, int timeoutMs) = 0;
};
