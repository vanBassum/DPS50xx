#pragma once
#include "rtos.h"
#include <cstdint>
#include <cstddef>
#include "ModbusClient.h"

class ModbusMaster
{
    inline constexpr static const char* TAG = "ModbusMaster";
public:
    explicit ModbusMaster(ModbusClient &client);

    ModbusError ReadCoil(uint8_t unitAddr, uint16_t coilAddress, bool *val, int timeoutMs);
    ModbusError WriteCoil(uint8_t unitAddr, uint16_t coilAddress, bool val, int timeoutMs);
    ModbusError ReadDiscreteInput(uint8_t unitAddr, uint16_t coilAddress, bool *val, int timeoutMs);
    ModbusError ReadInputRegister(uint8_t unitAddr, uint16_t registerAddress, uint16_t *val, int timeoutMs);
    ModbusError ReadHoldingRegister(uint8_t unitAddr, uint16_t registerAddress, uint16_t *val, int timeoutMs);
    ModbusError WriteHoldingRegister(uint8_t unitAddr, uint16_t registerAddress, uint16_t val, int timeoutMs);

    // Read multiple consecutive holding registers in a single transaction
    ModbusError ReadHoldingRegisters(uint8_t unitAddr, uint16_t startAddress, uint16_t quantity, uint16_t *vals, int timeoutMs);

private:
    ModbusClient &client;
    RecursiveMutex mutex;
};
