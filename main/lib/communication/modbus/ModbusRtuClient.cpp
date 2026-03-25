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
// ReadExact: read exactly `count` bytes, retrying until timeout
// -------------------------
bool ModbusRtuClient::ReadExact(uint8_t *buf, int count, TickType_t timeout)
{
    int total = 0;
    TickType_t start = xTaskGetTickCount();

    while (total < count)
    {
        TickType_t elapsed = xTaskGetTickCount() - start;
        if (elapsed >= timeout)
            return false;

        TickType_t remaining = timeout - elapsed;
        int n = uart_read_bytes(port_, buf + total, count - total, remaining);
        if (n <= 0)
            return false;
        total += n;
    }
    return true;
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
// Uses deterministic frame-size detection instead of inter-character timeout
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
    // Read response using deterministic frame-size detection
    // Step 1: Read unitId + function code (2 bytes)
    // Step 2: Determine expected frame size from function code
    // Step 3: Read remaining bytes
    // -------------------------
    TickType_t rxTimeout = pdMS_TO_TICKS(timeoutMs);
    int rxPtr = 0;

    // Read unitId + function (2 bytes)
    if (!ReadExact(rxBuffer_, 2, rxTimeout))
        return ModbusError::Timeout;
    rxPtr = 2;

    uint8_t func = rxBuffer_[1];
    int frameSize;

    if (func & 0x80)
    {
        // Exception response: [unitId][func|0x80][exCode][CRC_lo][CRC_hi] = 5
        frameSize = 5;
    }
    else
    {
        switch (func)
        {
            case 0x01: // Read Coils
            case 0x02: // Read Discrete Inputs
            case 0x03: // Read Holding Registers
            case 0x04: // Read Input Registers
                // Variable length: need byteCount to determine frame size
                if (!ReadExact(rxBuffer_ + 2, 1, rxTimeout))
                    return ModbusError::Timeout;
                rxPtr = 3;
                // Total: unitId + func + byteCount + data[byteCount] + CRC(2)
                frameSize = 3 + rxBuffer_[2] + 2;
                break;

            case 0x05: // Write Single Coil
            case 0x06: // Write Single Register
            case 0x0F: // Write Multiple Coils
            case 0x10: // Write Multiple Registers
                // Fixed echo: [unitId][func][addr_hi][addr_lo][val_hi][val_lo][CRC_lo][CRC_hi] = 8
                frameSize = 8;
                break;

            default:
                ESP_LOGW(TAG, "Unknown function 0x%02X in response", func);
                return ModbusError::InvalidReplyFunctionCode;
        }
    }

    // Sanity check
    if (frameSize > (int)sizeof(rxBuffer_))
        return ModbusError::InsufficientRxBufferSize;

    // Read remaining bytes
    int remaining = frameSize - rxPtr;
    if (remaining > 0)
    {
        if (!ReadExact(rxBuffer_ + rxPtr, remaining, rxTimeout))
        {
            ESP_LOGW(TAG, "Incomplete frame: got %d/%d bytes", rxPtr, frameSize);
            return ModbusError::Timeout;
        }
        rxPtr += remaining;
    }

    // -------------------------
    // CRC check
    // -------------------------
    uint16_t recvCrc = (uint16_t)rxBuffer_[rxPtr - 2] | ((uint16_t)rxBuffer_[rxPtr - 1] << 8);
    uint16_t crcCheck = CalculateCRC(rxBuffer_, rxPtr - 2);
    if (recvCrc != crcCheck)
    {
        ESP_LOGW(TAG, "CRC mismatch: recv=0x%04X calc=0x%04X (%d bytes)", recvCrc, crcCheck, rxPtr);
        return ModbusError::InvalidCRC;
    }

    // -------------------------
    // UnitId check
    // -------------------------
    if (rxBuffer_[0] != unitId)
        return ModbusError::InvalidDeviceReply_UnitId;

    // -------------------------
    // Exception handling
    // -------------------------
    if (func & 0x80)
    {
        uint8_t exceptionCode = rxBuffer_[2];
        return MapExceptionCode(exceptionCode);
    }

    if (func != request.functionCode)
        return ModbusError::InvalidReplyFunctionCode;

    // -------------------------
    // Deserialize PDU (skip unitId, exclude CRC)
    // -------------------------
    size_t pduLen = (size_t)rxPtr - 3; // remove unitId (1) + CRC (2)
    response = ModbusPdu::Deserialize(&rxBuffer_[1], pduLen);

    return ModbusError::NoError;
}
