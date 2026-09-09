#pragma once

// ──────────────────────────────────────────────────────────────
// Board configuration — ESP32-C3 SuperMini wired to an XY6020L.
//
// Same MCU and same LED as dps50xx_c3; what differs is the supply on the wire,
// and with it two things that are NOT the driver's business: the pins and the
// baud rate.
// ──────────────────────────────────────────────────────────────

#include <cstdint>

namespace BoardConfig
{
    // ── Status LED ──
    // C3 SuperMini: built-in LED on GPIO8, active LOW.
    static constexpr int  LED_PIN         = 8;
    static constexpr bool LED_ACTIVE_HIGH = false;

    // ── Modbus RTU to the XY6020L ──
    // The wiring is stated in the SUPPLY's terms: GPIO4 carries the XY6020L's
    // TX and GPIO3 its RX. So from this board's side that is RX 4 / TX 3 — the
    // crossover is the one thing here that silently produces a dead link rather
    // than a wrong reading, so if the supply never answers, swap these two
    // before suspecting anything else.
    static constexpr int      MODBUS_TX_PIN    = 3;   // → XY6020L RX
    static constexpr int      MODBUS_RX_PIN    = 4;   // ← XY6020L TX

    // 115200, where a DPS50xx runs at 9600 — the XY6020L's factory default.
    // Register 0x0019 can change it, and this driver deliberately never reads
    // or writes that register.
    static constexpr uint32_t MODBUS_BAUD      = 115200;
    static constexpr int      MODBUS_UART_PORT = 1;   // UART_NUM_1

    // ⚠ GPIO3/4 are UNVERIFIED for WiFi on this board, and that is a separate
    // question from whether the UART works. This module's ceramic antenna is
    // placed without its datasheet keep-out, so conductors near it detune it:
    // 21/20 and 0/1 both stopped the station holding an association at all,
    // with the UART never configured and the pads left high-impedance — a
    // passive wire detunes as well as a driven one. 6/5 is the only pair proven
    // to associate (see dps50xx_c3/BoardConfig.h and
    // reasoning/2026-08-26-21h19). GPIO3/4 sit next to 0/1 on the header, so
    // watch the first association: "associated, never got an address" is the
    // signature, and stray wire ROUTING matters as much as the pad chosen.
    static constexpr uint8_t PSU_UNIT_ADDRESS = 1;
}
