#pragma once
#include "ModbusClient.h"
#include "driver/uart.h"
#include <cstdint>

class ModbusRtuClient : public ModbusClient
{
public:
    ModbusRtuClient() = default;

    void Init(int txPin, int rxPin, uint32_t baud = 9600, uart_port_t port = UART_NUM_1);
    bool Connect(int timeoutMs) override { return true; }
    void Disconnect() override {}
    ModbusError Execute(uint8_t unitId, const ModbusPdu &request, ModbusPdu &response, int timeoutMs) override;

private:
    static uint16_t CalculateCRC(const uint8_t *buf, int len);
    bool ReadExact(uint8_t *buf, int count, TickType_t timeout);

    uart_port_t port_ = UART_NUM_1;
    uint8_t rxBuffer_[256];
};
