#pragma once

#include "interfaces/Psu.h"
#include "ModbusMaster.h"
#include <cstdint>
#include <cstddef>

// ──────────────────────────────────────────────────────────────
// The Psu role over a Modbus RTU wire: the chain walk that turns a register
// table into transactions, and the retry/offline policy that a supply sharing
// its MCU with a front panel needs.
//
// This is where the bus knowledge lives, which is why it is a driver and not
// part of interfaces/Psu.h — a role must not oblige every future supply to be
// a Modbus one.
//
// A concrete supply derives from this and its ENTIRE body is a register table
// (see DPS5020.h). Nothing per-supply happens here and nothing here knows a
// register address.
// ──────────────────────────────────────────────────────────────

/// Which half of a two-register value comes first. There is no convention worth
/// trusting here: the DPS50xx has no 32-bit register at all, and the XY6020L
/// puts the LOW word at the lower address (its map names 0x0006 charge and
/// 0x0007 charge-high). So the table says which, per register.
enum class PsuWordOrder : uint8_t
{
    HighFirst,
    LowFirst,
};

/// One reading that lives at a Modbus holding-register address. Adds to
/// PsuReading exactly what the wire needs and the application never asked for.
struct PsuRegister : PsuReading
{
    const uint16_t     address;
    const uint8_t      words;   ///< 1, or 2 for a 32-bit value in a register pair
    const float        scale;   ///< raw * scale = value, e.g. 0.01
    const PsuWordOrder order;   ///< only meaningful when words == 2

protected:
    PsuRegister(const char* key, const char* label, const char* unit, const char* telemKey,
                PsuKind kind, PsuAccess access, float min, float max,
                const char* const* enumLabels, uint8_t enumCount,
                uint16_t address, uint8_t words, float scale,
                PsuWordOrder order = PsuWordOrder::HighFirst)
        : PsuReading(key, label, unit, telemKey, kind, access, min, max, enumLabels, enumCount),
          address(address), words(words), scale(scale), order(order) {}
};

// ── Table sugar ───────────────────────────────────────────────
// Five names instead of one with a pile of defaults, because the table in a
// driver is the thing people will read: `WriteNumber` says at a glance what
// `PsuAccess::ReadWrite` in argument position seven does not.

/// A read-only measurement. `words = 2` covers the 32-bit accumulators an
/// XY6020L keeps (Ah, Wh); pass the word order with it, and see PsuWordOrder for
/// why there is no default worth trusting.
struct ReadNumber : PsuRegister
{
    ReadNumber(const char* key, const char* label, const char* unit,
               uint16_t address, float scale, const char* telemKey = nullptr,
               uint8_t words = 1, PsuWordOrder order = PsuWordOrder::HighFirst)
        : PsuRegister(key, label, unit, telemKey, PsuKind::Number, PsuAccess::Read,
                      0.0f, 0.0f, nullptr, 0, address, words, scale, order) {}
};

/// A setpoint. min/max are the supply's own range and travel to the UI, which is
/// why no MAX_VOLTAGE constant exists in the application any more.
struct WriteNumber : PsuRegister
{
    WriteNumber(const char* key, const char* label, const char* unit,
                uint16_t address, float scale, float min, float max,
                const char* telemKey = nullptr)
        : PsuRegister(key, label, unit, telemKey, PsuKind::Number, PsuAccess::ReadWrite,
                      min, max, nullptr, 0, address, 1, scale) {}
};

struct ReadBool : PsuRegister
{
    ReadBool(const char* key, const char* label, uint16_t address,
             const char* telemKey = nullptr)
        : PsuRegister(key, label, "", telemKey, PsuKind::Bool, PsuAccess::Read,
                      0.0f, 0.0f, nullptr, 0, address, 1, 1.0f) {}
};

struct WriteBool : PsuRegister
{
    WriteBool(const char* key, const char* label, uint16_t address,
              const char* telemKey = nullptr)
        : PsuRegister(key, label, "", telemKey, PsuKind::Bool, PsuAccess::ReadWrite,
                      0.0f, 1.0f, nullptr, 0, address, 1, 1.0f) {}
};

/// A register code with the supply's own names for it. The labels are the
/// driver's, so a supply with more protection states than another needs no
/// agreement from the application or the frontend.
struct ReadEnum : PsuRegister
{
    template <size_t N>
    ReadEnum(const char* key, const char* label, uint16_t address,
             const char* const (&labels)[N])
        : PsuRegister(key, label, "", nullptr, PsuKind::Enum, PsuAccess::Read,
                      0.0f, 0.0f, labels, static_cast<uint8_t>(N), address, 1, 1.0f)
    {
        static_assert(N <= 255, "enum label table too long");
    }
};

// ──────────────────────────────────────────────────────────────
class ModbusPsu : public Psu
{
public:
    ModbusPsu(ModbusMaster& master, uint8_t address, const char* tag)
        : master_(master), address_(address), tag_(tag) {}

    /// Reads every registered register, in as few transactions as the table
    /// allows: contiguous addresses are coalesced into one batch read, so a map
    /// like the DPS5020's costs exactly one round trip.
    ModbusError Poll() override;

    bool IsOnline() const override { return online_; }

    ModbusError Write(PsuReading& reading, float value) override;

    /// Overriding one Write() would otherwise hide the key-based one from
    /// anything holding a DPS5020& rather than a Psu&.
    using Psu::Write;

    uint8_t UnitAddress() const { return address_; }

protected:
    static constexpr int TIMEOUT_MS = 500;
    static constexpr int MAX_ATTEMPTS = 1;      ///< per transaction
    static constexpr int OFFLINE_THRESHOLD = 3; ///< consecutive failed polls
    static constexpr uint16_t MAX_RUN = 32;     ///< registers per transaction

private:
    ModbusError ReadRun(uint16_t start, uint16_t count, uint16_t* out);

    ModbusMaster& master_;
    uint8_t       address_;
    const char*   tag_;
    bool          online_ = false;
    int           failCount_ = 0;
};
