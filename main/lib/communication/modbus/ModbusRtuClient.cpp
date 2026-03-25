#include "ModbusRtuClient.h"
#include "ModbusError.h"
#include "esp_log.h"

#include <cstring>
#include <cstdio>

static const char *TAG = "ModbusRtu";

// -------------------------
// CRC-16 (Modbus RTU)
// -------------------------
uint16_t ModbusRtuClient::CalculateCRC(const uint8_t *buf, int len)
{
    uint16_t crc = 0xFFFF;
    for (int pos = 0; pos < len; pos++)
    {
        crc ^= static_cast<uint16_t>(buf[pos]);
        for (int i = 0; i < 8; i++)
        {
            if (crc & 0x0001)
            {
                crc >>= 1;
                crc ^= 0xA001;
            }
            else
            {
                crc >>= 1;
            }
        }
    }
    return crc;
}

// -------------------------
// Init: configure ESP-IDF UART
// -------------------------
void ModbusRtuClient::Init(int txPin, int rxPin, uint32_t baud, uart_port_t port)
{
    port_ = port;

    uart_config_t uartConfig = {};
    uartConfig.baud_rate = (int)baud;
    uartConfig.data_bits = UART_DATA_8_BITS;
    uartConfig.parity = UART_PARITY_DISABLE;
    uartConfig.stop_bits = UART_STOP_BITS_1;
    uartConfig.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
    uartConfig.source_clk = UART_SCLK_DEFAULT;

    ESP_ERROR_CHECK(uart_driver_install(port_, 256, 256, 0, nullptr, 0));
    ESP_ERROR_CHECK(uart_param_config(port_, &uartConfig));
    ESP_ERROR_CHECK(uart_set_pin(port_, txPin, rxPin, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));

    ESP_LOGI(TAG, "UART%d init: TX=%d RX=%d baud=%lu", (int)port_, txPin, rxPin, (unsigned long)baud);
}

// -------------------------
// Helpers
// -------------------------
static ModbusError MapExceptionCode(uint8_t ex)
{
    switch (ex)
    {
        case 0x01: return ModbusError::IllegalFunction;
        case 0x02: return ModbusError::IllegalDataAddress;
        case 0x03: return ModbusError::IllegalDataValue;
        case 0x04: return ModbusError::SlaveDeviceFailure;
        case 0x05: return ModbusError::Acknowledge;
        case 0x06: return ModbusError::SlaveDeviceBusy;
        case 0x08: return ModbusError::MemoryParityError;
        case 0x0A: return ModbusError::GatewayPathUnavailable;
        case 0x0B: return ModbusError::GatewayTargetFailed;
        default:   return ModbusError::UnknownException;
    }
}

// -------------------------
// Execute: send request, receive response via UART
// -------------------------
ModbusError ModbusRtuClient::Execute(uint8_t unitId,
                                     const ModbusPdu &request,
                                     ModbusPdu &response,
                                     int timeoutMs)
{
    // -------------------------
    // Build TX frame: [unitId][PDU...][CRClo][CRChi]
    // -------------------------
    uint8_t txFrame[ModbusPdu::MaxPduSize + 3];
    int wrPtr = 0;

    txFrame[wrPtr++] = unitId;
    wrPtr += request.Serialize(&txFrame[wrPtr], sizeof(txFrame) - wrPtr);

    if (wrPtr < 2)
        return ModbusError::InvalidArguments;

    uint16_t crc = CalculateCRC(txFrame, wrPtr);
    txFrame[wrPtr++] = (uint8_t)(crc & 0xFF);
    txFrame[wrPtr++] = (uint8_t)((crc >> 8) & 0xFF);

    // -------------------------
    // Flush RX, send frame, wait for TX complete
    // -------------------------
    uart_flush_input(port_);
    uart_write_bytes(port_, txFrame, wrPtr);
    uart_wait_tx_done(port_, pdMS_TO_TICKS(100));

    // -------------------------
    // Read response using inter-character timeout
    // Standard Modbus RTU: frame ends after 3.5 character silence
    // At 9600 baud: ~4ms, we use 10ms for safety
    // -------------------------
    int rxPtr = 0;
    const TickType_t interCharTimeout = pdMS_TO_TICKS(10);

    // Wait for first byte with full timeout
    int n = uart_read_bytes(port_, &rxBuffer_[0], 1, pdMS_TO_TICKS(timeoutMs));
    if (n != 1)
        return ModbusError::Timeout;
    rxPtr = 1;

    // Read remaining bytes until inter-character timeout (frame complete)
    while (rxPtr < (int)sizeof(rxBuffer_))
    {
        n = uart_read_bytes(port_, &rxBuffer_[rxPtr], 1, interCharTimeout);
        if (n != 1)
            break;
        rxPtr++;
    }

    // -------------------------
    // Validate minimum reply length
    // -------------------------
    if (rxPtr < 5)
    {
        ESP_LOGW(TAG, "Short frame: %d bytes", rxPtr);
        return ModbusError::InvalidReplyLength;
    }

    // -------------------------
    // CRC check
    // -------------------------
    uint16_t recvCrc = (uint16_t)rxBuffer_[rxPtr - 2] | ((uint16_t)rxBuffer_[rxPtr - 1] << 8);
    uint16_t crcCheck = CalculateCRC(rxBuffer_, rxPtr - 2);
    if (recvCrc != crcCheck)
    {
        ESP_LOGW(TAG, "CRC mismatch: recv=0x%04X calc=0x%04X", recvCrc, crcCheck);
        return ModbusError::InvalidCRC;
    }

    // -------------------------
    // UnitId check
    // -------------------------
    if (rxBuffer_[0] != unitId)
        return ModbusError::InvalidDeviceReply_UnitId;

    // -------------------------
    // Function / exception handling
    // -------------------------
    uint8_t func = rxBuffer_[1];

    // Exception response: [unit][func|0x80][ex][CRClo][CRChi]
    if (func & 0x80)
    {
        if (rxPtr != 5)
            return ModbusError::InvalidDeviceReply_ReplyLength;

        uint8_t exceptionCode = rxBuffer_[2];
        return MapExceptionCode(exceptionCode);
    }

    if (func != request.functionCode)
        return ModbusError::InvalidReplyFunctionCode;

    // -------------------------
    // Deserialize PDU (skip unitId byte, exclude CRC)
    // -------------------------
    size_t pduLen = (size_t)rxPtr - 3; // remove unitId (1) + CRC (2)
    response = ModbusPdu::Deserialize(&rxBuffer_[1], pduLen);

    // Sanity-check read response lengths
    if ((func == 0x03 || func == 0x04))
    {
        uint8_t byteCount = rxBuffer_[2];
        if (rxPtr != (int)(3 + byteCount + 2))
            return ModbusError::InvalidReplyLength;
    }
    else if (func == 0x06 || func == 0x10)
    {
        if (rxPtr != 8)
            return ModbusError::InvalidReplyLength;
    }

    return ModbusError::NoError;
}
