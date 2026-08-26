#pragma once

// ──────────────────────────────────────────────────────────────
// Board configuration — ESP32-C3 SuperMini wired to a DPS50xx front panel.
//
// Same roles as dps50xx_esp32, different pins and an inverted LED. Kept as a
// separate folder rather than an #if inside one BoardConfig.h: -DBOARD= is
// what picks between them, and only the chosen folder is on the include path.
// ──────────────────────────────────────────────────────────────

#include <cstdint>

namespace BoardConfig
{
    // ── Status LED ──
    // C3 SuperMini: built-in LED on GPIO8, active LOW.
    static constexpr int  LED_PIN         = 8;
    static constexpr bool LED_ACTIVE_HIGH = false;

    // ── Modbus RTU to the DPS50xx ──
    // 0/1 rather than the 21/20 this started on, matching the bench wiring. The move
    // was made chasing a WiFi fault and did NOT fix it: with the DPS wired to this
    // board, the station fails to hold an association on either pin pair. That fault
    // is not in this file and not in the firmware at all — it reproduces with the
    // UART never configured, the poll task never started, and these pads left in
    // their power-on high-impedance state. Pick whichever pair suits the wiring.
    static constexpr int      MODBUS_TX_PIN    = 0;
    static constexpr int      MODBUS_RX_PIN    = 1;
    static constexpr uint32_t MODBUS_BAUD      = 9600;
    static constexpr int      MODBUS_UART_PORT = 1;   // UART_NUM_1

    // Modbus unit address of the supply. Factory default is 1.
    static constexpr uint8_t DPS_UNIT_ADDRESS = 1;
}
