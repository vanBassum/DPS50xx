#pragma once

#include "interfaces/Psu.h"
#include "ModbusMaster.h"
#include <cstdint>
#include <cstddef>

// ──────────────────────────────────────────────────────────────
// The Psu role over a Modbus RTU wire.
//
// This is the TRANSPORT layer, and the separation from interfaces/Psu.h is the
// point: a register is a place a device stores something, a capability is
// something the supply can do, and this file is where the first is turned into
// the second. Registers never leave here.
//
//     ModbusReg   — an address, a scale, a word order. Storage.
//     BoundCapability — a capability that happens to sit on one register.
//     ModbusReg with no target — read for the driver's own arithmetic, and
//                                published NOWHERE. Not a capability.
//
// So `Poll()` walks the REGISTER list, not the capability list: it reads
// whatever addresses the driver asked for, hands each register's value to the
// capability behind it if there is one, and then calls Normalize() so the driver
// can compute the capabilities that no single register holds.
//
// A concrete supply derives from this and its body is a table plus, if its
// protocol needs it, a Normalize() override. See XY6020L.
// ──────────────────────────────────────────────────────────────

/// Which half of a two-register value comes first. There is no convention worth
/// trusting: the DPS50xx has no 32-bit register at all, and the XY6020L puts the
/// LOW word at the lower address. So the table says which, per register.
enum class PsuWordOrder : uint8_t
{
    HighFirst,
    LowFirst,
};

/// One holding register: where a device keeps something, and what was last read
/// from it. An aggregate on purpose, so a table reads `Reg{0x0002, 0.01f}` and
/// the modifiers below compose onto it.
struct ModbusReg
{
    uint16_t     address = 0;
    float        scale = 1.0f;              ///< raw * scale = the value in units
    uint8_t      words = 1;                 ///< 2 for a 32-bit register pair
    PsuWordOrder order = PsuWordOrder::HighFirst;

    /// A raw value that means "no measurement" rather than a measurement — an
    /// unconnected XY6020L probe answers 8888, which is not 888.8 °C. -1 for a
    /// register with no such sentinel. Declared by the driver that knows the
    /// protocol, so no UI ever learns the number.
    int32_t sentinel = -1;

    /// False for a block whose absence must not condemn the supply: an
    /// unverified address, or configuration a poll can do without. A failed
    /// non-essential read makes its capabilities unavailable and the poll
    /// carries on, where an essential failure is what "offline" means.
    bool essential = true;

    // ── Live state, written by Poll ──
    uint32_t raw = 0;
    bool     answered = false;

    // ── Linkage, owned by ModbusPsu ──
    PsuCapability* target = nullptr;   ///< nullptr = the driver's own business
    ModbusReg*     next = nullptr;

    double Value() const { return static_cast<double>(raw) * static_cast<double>(scale); }
    bool IsSentinel() const { return sentinel >= 0 && raw == static_cast<uint32_t>(sentinel); }

    // Modifiers, so the wire facts compose instead of becoming argument nine.
    ModbusReg Pair(PsuWordOrder o) const { ModbusReg r = *this; r.words = 2; r.order = o; return r; }
    ModbusReg Sentinel(int32_t s) const  { ModbusReg r = *this; r.sentinel = s; return r; }
    ModbusReg Optional() const           { ModbusReg r = *this; r.essential = false; return r; }
};

using Reg = ModbusReg;

/// A capability whose value comes from exactly one register. The register is a
/// member, not a base class — a capability is not a kind of register.
struct BoundCapability : PsuCapability
{
    ModbusReg reg;

protected:
    BoundCapability(const char* key, const char* label, const char* unit, const char* telemKey,
                    PsuKind kind, PsuGroup group, PsuAccess access,
                    float min, float max,
                    const char* const* enumLabels, uint8_t enumCount,
                    ModbusReg reg)
        : PsuCapability(key, label, unit, telemKey, kind, group, access,
                        min, max, enumLabels, enumCount),
          reg(reg) {}
};

// ── Table sugar ───────────────────────────────────────────────
// Named types rather than one constructor with a tail of defaults, because a
// driver's table is the thing people read: `Limit` says at a glance what
// `PsuGroup::Protection, ReadWrite, essential=false` in argument position nine
// does not. Each name fixes the group and the access; only the wire facts and
// the range stay as parameters.

/// A live read-only measurement: the dashboard proper.
struct Measurement : BoundCapability
{
    Measurement(const char* key, const char* label, const char* unit,
                ModbusReg reg, const char* telemKey = nullptr)
        : BoundCapability(key, label, unit, telemKey, PsuKind::Number, PsuGroup::Core,
                          PsuAccess::Read, 0.0f, 0.0f, nullptr, 0, reg) {}
};

/// A setpoint. min/max are the supply's own range and travel to the UI.
struct Setpoint : BoundCapability
{
    Setpoint(const char* key, const char* label, const char* unit,
             ModbusReg reg, float min, float max, const char* telemKey = nullptr)
        : BoundCapability(key, label, unit, telemKey, PsuKind::Number, PsuGroup::Core,
                          PsuAccess::ReadWrite, min, max, nullptr, 0, reg) {}
};

/// A read-only state: CV/CC, and friends.
struct StateFlag : BoundCapability
{
    StateFlag(const char* key, const char* label, ModbusReg reg, const char* telemKey = nullptr)
        : BoundCapability(key, label, "", telemKey, PsuKind::Bool, PsuGroup::Core,
                          PsuAccess::Read, 0.0f, 0.0f, nullptr, 0, reg) {}
};

/// Something the operator can turn on and off.
struct Switch : BoundCapability
{
    Switch(const char* key, const char* label, ModbusReg reg, const char* telemKey = nullptr)
        : BoundCapability(key, label, "", telemKey, PsuKind::Bool, PsuGroup::Core,
                          PsuAccess::ReadWrite, 0.0f, 1.0f, nullptr, 0, reg) {}
};

/// A discrete choice with the supply's own names for its codes. The label TABLE
/// travels in `psu get`, which is what lets a UI draw a selector without being
/// told that 0 means M0. Read-only: `Selector` reports, it does not recall.
struct Selector : BoundCapability
{
    template <size_t N>
    Selector(const char* key, const char* label, ModbusReg reg,
             const char* const (&labels)[N])
        : BoundCapability(key, label, "", nullptr, PsuKind::Enum, PsuGroup::Core,
                          PsuAccess::Read, 0.0f, static_cast<float>(N - 1),
                          labels, static_cast<uint8_t>(N), reg)
    {
        static_assert(N <= 255, "label table too long");
    }
};

/// Accumulated since power-on: charge, energy, temperatures. Same wire shape as
/// a Measurement, different section of the UI — and the one place a sentinel is
/// common, because probes go missing.
struct SessionValue : BoundCapability
{
    SessionValue(const char* key, const char* label, const char* unit,
                 ModbusReg reg, const char* telemKey = nullptr)
        : BoundCapability(key, label, unit, telemKey, PsuKind::Number, PsuGroup::Session,
                          PsuAccess::Read, 0.0f, 0.0f, nullptr, 0, reg) {}
};

/// A configured trip threshold — not a measurement, and never on the dashboard.
struct Limit : BoundCapability
{
    Limit(const char* key, const char* label, const char* unit,
          ModbusReg reg, float min, float max)
        : BoundCapability(key, label, unit, nullptr, PsuKind::Number, PsuGroup::Protection,
                          PsuAccess::ReadWrite, min, max, nullptr, 0, reg) {}
};

struct LimitSwitch : BoundCapability
{
    LimitSwitch(const char* key, const char* label, ModbusReg reg)
        : BoundCapability(key, label, "", nullptr, PsuKind::Bool, PsuGroup::Protection,
                          PsuAccess::ReadWrite, 0.0f, 1.0f, nullptr, 0, reg) {}
};

/// Identity: model, firmware. Grouped so no dashboard shows it.
struct DeviceInfo : BoundCapability
{
    DeviceInfo(const char* key, const char* label, ModbusReg reg)
        : BoundCapability(key, label, "", nullptr, PsuKind::Number, PsuGroup::Info,
                          PsuAccess::Read, 0.0f, 0.0f, nullptr, 0, reg) {}
};

// ──────────────────────────────────────────────────────────────
class ModbusPsu : public Psu
{
public:
    ModbusPsu(ModbusMaster& master, uint8_t address, const char* tag)
        : master_(master), address_(address), tag_(tag) {}

    /// Reads every registered register, in as few transactions as the table
    /// allows: contiguous addresses of equal essentialness are coalesced, so a
    /// map like the DPS5020's costs exactly one round trip. Then Normalize().
    ModbusError Poll() override;

    bool IsOnline() const override { return online_; }

    ModbusError Write(PsuCapability& capability, float value) override;

    /// Overriding one Write() would otherwise hide the key-based one from
    /// anything holding a DPS5020& rather than a Psu&.
    using Psu::Write;

    uint8_t UnitAddress() const { return address_; }

protected:
    /// Called after a successful poll, before anything reads the chain: where a
    /// driver turns its own protocol's shape into the capabilities it offers.
    /// Default does nothing, which is right for a supply whose registers
    /// already mean what a capability means.
    virtual void Normalize() {}

    /// A capability that sits on one register: publishes it AND polls it.
    void Add(BoundCapability& c)
    {
        Psu::Add(c);
        c.reg.target = &c;
        LinkRegister(c.reg);
    }

    /// A register the driver reads for its own arithmetic. Polled, published
    /// nowhere — this is what stops an XY6020L's three on-time registers from
    /// becoming three fields in a UI.
    void Add(ModbusReg& reg)
    {
        reg.target = nullptr;
        LinkRegister(reg);
    }

    /// Derived capabilities (no register) go through the role's own Add.
    using Psu::Add;

    /// For Normalize(): the register list is the driver's, so a driver reads its
    /// own raws directly off the members it declared. Nothing here needs a
    /// lookup — this exists only for symmetry with Find().
    static constexpr int TIMEOUT_MS = 500;
    static constexpr int MAX_ATTEMPTS = 1;      ///< per transaction
    static constexpr int OFFLINE_THRESHOLD = 3; ///< consecutive failed polls
    static constexpr uint16_t MAX_RUN = 32;     ///< registers per transaction

private:
    void LinkRegister(ModbusReg& reg);
    ModbusError ReadRun(uint16_t start, uint16_t count, uint16_t* out, bool essential);
    ModbusReg* RegisterFor(const PsuCapability& capability) const;

    ModbusMaster& master_;
    uint8_t       address_;
    const char*   tag_;
    bool          online_ = false;
    int           failCount_ = 0;

    /// A non-essential block that never answers would otherwise log twice per
    /// poll forever. Complain once per transition instead.
    bool optionalFailed_ = false;

    ModbusReg* regHead_ = nullptr;
    ModbusReg* regTail_ = nullptr;
};
