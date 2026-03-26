#pragma once

//
// Target-specific pin assignments and hardware configuration.
// All porting / board-level constants live here.
//

#include "sdkconfig.h"

#if CONFIG_IDF_TARGET_ESP32C3

// ESP32-C3 SuperMini
static constexpr int MODBUS_TX_PIN = 21;
static constexpr int MODBUS_RX_PIN = 20;

#elif CONFIG_IDF_TARGET_ESP32

// ESP32 — TODO: set correct pins
static constexpr int MODBUS_TX_PIN = 17;
static constexpr int MODBUS_RX_PIN = 16;

#else
#error "Unsupported IDF target — add pin definitions to BoardConfig.h"
#endif

// Shared across all targets
static constexpr uint32_t MODBUS_BAUD = 9600;
static constexpr int MODBUS_UART_PORT = 1; // UART_NUM_1
