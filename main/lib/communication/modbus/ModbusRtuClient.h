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

    /// `received` reports how many bytes actually arrived before the deadline —
    /// the difference between a silent bus and a slave that is answering but not
    /// being heard, which is the first thing worth knowing about a timeout.
    bool ReadExact(uint8_t *buf, int count, TickType_t timeout, int *received = nullptr);

    /// Drain and report whatever was in the RX buffer when a request began: it is
    /// the previous response arriving after its deadline, and it is the only
    /// evidence that separates a slow slave from a broken wire.
    void ReportStaleBytes(size_t buffered, uint8_t unitId);

    uart_port_t port_ = UART_NUM_1;
    uint8_t rxBuffer_[256];
};
