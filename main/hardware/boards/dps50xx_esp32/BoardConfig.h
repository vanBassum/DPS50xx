#pragma once

// ──────────────────────────────────────────────────────────────
// Board configuration — ESP32 DevKit wired to a DPS50xx front panel.
//
// Pin assignments only, as constants. No #if CONFIG_IDF_TARGET here:
// a second target is a second board folder (see dps50xx_c3), which is
// what -DBOARD= selects. Only the chosen folder is on the include
// path, so `#include "BoardConfig.h"` resolves to exactly one of them.
// ──────────────────────────────────────────────────────────────

#include <cstdint>

namespace BoardConfig
{
    // ── Status LED ──
    // ESP32 DevKit: built-in LED on GPIO2, active high.
    static constexpr int  LED_PIN         = 2;
    static constexpr bool LED_ACTIVE_HIGH = true;

    // ── Modbus RTU to the DPS50xx ──
    // The supply's own MCU drives the display and the keypad off the same
    // core, so it misses requests under load — see DPS5020's retry loop.
    static constexpr int      MODBUS_TX_PIN    = 17;
    static constexpr int      MODBUS_RX_PIN    = 16;
    static constexpr uint32_t MODBUS_BAUD      = 9600;
    static constexpr int      MODBUS_UART_PORT = 1;   // UART_NUM_1

    // Modbus unit address of the supply. Factory default is 1; the front
    // panel can change it.
    static constexpr uint8_t DPS_UNIT_ADDRESS = 1;
}
