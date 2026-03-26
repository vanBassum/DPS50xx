#pragma once

// ──────────────────────────────────────────────────────────────
// Board configuration — hardware-specific pin assignments and constants.
// Edit this file to match your board or target MCU.
// ──────────────────────────────────────────────────────────────

#include "sdkconfig.h"

namespace BoardConfig
{
    // LED
    // Set to -1 if the board has no LED.
#if CONFIG_IDF_TARGET_ESP32C3
    // ESP32-C3 SuperMini: built-in LED on GPIO 8
    static constexpr int LED_PIN = 8;
    static constexpr bool LED_ACTIVE_HIGH = false;
#elif CONFIG_IDF_TARGET_ESP32
    // ESP32 DevKit: built-in LED on GPIO 2
    static constexpr int LED_PIN = 2;
    static constexpr bool LED_ACTIVE_HIGH = true;
#else
    static constexpr int LED_PIN = -1;
    static constexpr bool LED_ACTIVE_HIGH = true;
#endif

    // Modbus RTU (DPS5020)
#if CONFIG_IDF_TARGET_ESP32C3
    static constexpr int MODBUS_TX_PIN = 21;
    static constexpr int MODBUS_RX_PIN = 20;
#elif CONFIG_IDF_TARGET_ESP32
    static constexpr int MODBUS_TX_PIN = 17;
    static constexpr int MODBUS_RX_PIN = 16;
#else
#error "Unsupported IDF target — add pin definitions to BoardConfig.h"
#endif

    static constexpr uint32_t MODBUS_BAUD = 9600;
    static constexpr int MODBUS_UART_PORT = 1; // UART_NUM_1
}
