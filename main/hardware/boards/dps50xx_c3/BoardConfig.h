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
    // 6/5, and the choice is about the ANTENNA, not the UART. This board's ceramic
    // antenna is placed without the keep-out clearance its datasheet requires, so it
    // is detuned by conductors near it — a documented SuperMini flaw that names
    // GPIO20/21 specifically. Wiring the DPS to 21/20 (and then to 0/1) stopped the
    // station holding an association at all: it associated, never got an address,
    // and the web UI was unreachable. Same wires on 6/5: associates in 2.8 s, address
    // in 3.9 s, index.html served in 63 ms.
    //
    // It is not the firmware. The fault reproduced with the UART never configured,
    // the poll task never started, and the pads left in their power-on
    // high-impedance state — a passive wire detunes just as well as a driven one.
    // So when moving these, the question is distance from the antenna, and stray
    // wire ROUTING matters as much as which pad is chosen.
    static constexpr int      MODBUS_TX_PIN    = 6;
    static constexpr int      MODBUS_RX_PIN    = 5;
    static constexpr uint32_t MODBUS_BAUD      = 9600;
    static constexpr int      MODBUS_UART_PORT = 1;   // UART_NUM_1

    // Modbus unit address of the supply. Factory default is 1.
    static constexpr uint8_t DPS_UNIT_ADDRESS = 1;
}
