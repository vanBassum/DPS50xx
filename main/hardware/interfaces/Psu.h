#pragma once

#include "Fatal.h"
#include "ModbusError.h"
#include <cstdint>
#include <cstring>

// ──────────────────────────────────────────────────────────────
// Role interface: what a bench supply CAN DO, in the application's vocabulary.
//
// This file is the capability model, and it deliberately knows nothing about
// registers. A capability is a semantic thing — "output voltage", "runtime in
// seconds", "which preset is active" — and how a particular supply stores it is
// the driver's business:
//
//     device register map                     (transport/storage — ModbusPsu)
//         ↓  driver decoding & normalization
//     semantic PSU capabilities               (THIS FILE — the public interface)
//         ↓
//     manager / `psu get` / telemetry
//         ↓
//     frontend
//
// The two must not be conflated, and the giveaway when they are is a UI field
// that exists only because a register does. An XY6020L keeps its runtime in
// three registers (hours, minutes, seconds); the capability is ONE
// `runtimeSeconds`, and nothing above the driver ever learns there were three.
// A capability may map to one register, combine several, need scaling, be
// derived from other data, or have no register at all on the next supply.
//
// So: capabilities are DECLARED, not generated. A driver that reads a register
// has not thereby published anything — see ModbusPsu, where a register with no
// capability behind it is an ordinary and expected thing.
//
// Registration follows Setting and CommandEntry: the owner provides the memory,
// the entry IS the link, misuse fails on the first boot. Two deliberate
// differences from Setting:
//
//   • Capabilities are ORDINARY MEMBERS of the driver, not `inline static`.
//     A setting is static because there is one settings namespace per device;
//     capability VALUES are per instance, and two supplies on one bus sharing
//     static storage would be a silent, ugly bug. Do not "fix" this.
//   • The chain appends at the tail, where Setting's pushes at the head.
//     Declaration order is display order, and a dashboard that reordered itself
//     on every firmware change would be worse than the one extra pointer costs.
//
// ModbusError appears here, which looks like transport vocabulary leaking into
// a role. It is from `lib/` — layer-free substrate, like Stream — and a write
// reply says WHICH error it was, so flattening it to a bool would lose the one
// thing a caller can act on.
// ──────────────────────────────────────────────────────────────

class Psu;
class PsuCapabilityIterator;

/// How to interpret and render a capability's value. A generic consumer needs
/// this and nothing else — no downcasts, no per-type accessors.
enum class PsuKind : uint8_t
{
    Number,     ///< a measurement in `unit`, e.g. 12.48 V
    Bool,       ///< 0 or 1
    Enum,       ///< a discrete choice with a label table — a selector, not a number
    Duration,   ///< a whole number of SECONDS; the reader formats it (HH:MM:SS)
};

/// Where a capability belongs, decided by the driver that knows what it MEANS.
/// Without this a frontend has to keep its own list of keys and guess — which is
/// how "Memory Group 0.00" ended up on a dashboard.
enum class PsuGroup : uint8_t
{
    Core,        ///< live measurements and setpoints: the dashboard proper
    Session,     ///< accumulated since power-on — charge, energy, runtime, temps
    Protection,  ///< configured trip thresholds. Not measurements, not live.
    Info,        ///< identity: model, firmware. Nobody watches these.
};

enum class PsuAccess : uint8_t
{
    Read,
    ReadWrite,
};

// ──────────────────────────────────────────────────────────────
// One capability: what it means, how to render it, and its current value.
// Carries no address, no scale and no word order — a capability that needed
// those would be a register wearing a capability's name.
// ──────────────────────────────────────────────────────────────
struct PsuCapability
{
    const char* const key;      ///< semantic name — stable, this is API
    const char* const label;    ///< shown in a UI
    const char* const unit;     ///< "" where there is none
    const char* const telemKey; ///< telemetry field name; nullptr = not recorded

    const PsuKind   kind;
    const PsuGroup  group;
    const PsuAccess access;

    /// Writable range, in the same units as `value`; meaningless when read-only.
    /// This is where MAX_VOLTAGE/MAX_CURRENT went — the limit belongs to the
    /// supply that has it, and it travels to the UI so the browser stops
    /// hardcoding 50 and 20.
    const float min;
    const float max;

    const char* const* const enumLabels;  ///< Enum only, else nullptr
    const uint8_t            enumCount;

    /// Double, not float: a Duration is a whole number of seconds and must stay
    /// exact. A double is exact to 2^53; a float starts losing whole seconds
    /// after about 194 days of runtime.
    double value = 0;

    /// False when the supply has no value for this right now — an unconnected
    /// probe, or a capability whose registers did not answer. A UI shows "—"
    /// rather than presenting a sentinel as a measurement.
    bool available = true;

    bool AsBool() const { return value != 0.0; }
    bool Writable() const { return access == PsuAccess::ReadWrite; }
    bool InRange(float v) const { return v >= min && v <= max; }

    /// The label for the code currently held, or nullptr for every other kind.
    /// This is what killed PROTECTION_LABELS in the frontend: the supply that
    /// defines the codes is the thing that names them.
    const char* ValueLabel() const
    {
        if (kind != PsuKind::Enum || enumLabels == nullptr)
            return nullptr;
        const auto i = static_cast<uint8_t>(value);
        return i < enumCount ? enumLabels[i] : "?";
    }

    /// What a driver calls once it has decided the value — from a register, from
    /// several, or from nothing at all.
    void Set(double v) { value = v; available = true; }
    void SetUnavailable() { value = 0; available = false; }

    PsuCapability(const PsuCapability&) = delete;
    PsuCapability& operator=(const PsuCapability&) = delete;

    /// A registered capability is a live chain link; letting it die would leave
    /// a dangling pointer behind. Same reasoning as ~Setting and ~CommandEntry.
    virtual ~PsuCapability()
    {
        if (registered)
            FATAL("registered capability '%s' destroyed — a supply's capability "
                  "table must live as long as its driver", key);
    }

protected:
    PsuCapability(const char* key, const char* label, const char* unit, const char* telemKey,
                  PsuKind kind, PsuGroup group, PsuAccess access,
                  float min, float max,
                  const char* const* enumLabels, uint8_t enumCount)
        : key(key), label(label), unit(unit), telemKey(telemKey),
          kind(kind), group(group), access(access), min(min), max(max),
          enumLabels(enumLabels), enumCount(enumCount) {}

private:
    friend class Psu;
    friend class PsuCapabilityIterator;

    PsuCapability* next = nullptr;
    bool registered = false;
};

/// A capability with nothing behind it but arithmetic: the driver computes it in
/// its own normalization step. `runtimeSeconds` from three registers is the
/// canonical case, and the reason the capability model exists at all.
struct DerivedCapability : PsuCapability
{
    DerivedCapability(const char* key, const char* label, const char* unit,
                      PsuKind kind, PsuGroup group, const char* telemKey = nullptr)
        : PsuCapability(key, label, unit, telemKey, kind, group, PsuAccess::Read,
                        0.0f, 0.0f, nullptr, 0) {}
};

// ──────────────────────────────────────────────────────────────
// Iteration — the only public way to walk the chain, which is what lets `next`
// stay private. Minimal forward iterator: `for (PsuCapability& c : psu)`.
// ──────────────────────────────────────────────────────────────
class PsuCapabilityIterator
{
    PsuCapability* cur_;

public:
    explicit PsuCapabilityIterator(PsuCapability* c) : cur_(c) {}

    PsuCapability& operator*() const { return *cur_; }
    PsuCapabilityIterator& operator++() { cur_ = cur_->next; return *this; }
    bool operator!=(const PsuCapabilityIterator& o) const { return cur_ != o.cur_; }
    bool operator==(const PsuCapabilityIterator& o) const { return cur_ == o.cur_; }
};

// ──────────────────────────────────────────────────────────────
// The role. Owns the capability chain (so the link can stay private) and leaves
// every question of transport to the implementation.
// ──────────────────────────────────────────────────────────────
class Psu
{
public:
    virtual ~Psu() = default;

    Psu(const Psu&) = delete;
    Psu& operator=(const Psu&) = delete;
    Psu(Psu&&) = delete;
    Psu& operator=(Psu&&) = delete;

    /// Refresh every capability. The error is returned for logging; whether the
    /// supply is CONSIDERED gone is IsOnline()'s business, because one missed
    /// transaction is normal on a supply whose MCU also drives its front panel.
    virtual ModbusError Poll() = 0;

    virtual bool IsOnline() const = 0;

    /// Write one capability, in its own units. The implementation decides what
    /// that means on the wire, and updates `value` only if the supply took it.
    virtual ModbusError Write(PsuCapability& capability, float value) = 0;

    // ── Lookup by semantic key ────────────────────────────────
    // A short chain walk with strcmp, which is free at poll rates. A hot
    // consumer should hold the pointer Find() returns rather than looking the
    // same key up in a loop.

    PsuCapability* Find(const char* key) const
    {
        for (PsuCapability* c = head_; c != nullptr; c = c->next)
            if (std::strcmp(c->key, key) == 0)
                return c;
        return nullptr;
    }

    bool Has(const char* key) const { return Find(key) != nullptr; }

    float Number(const char* key, float fallback = 0.0f) const
    {
        const PsuCapability* c = Find(key);
        return c ? static_cast<float>(c->value) : fallback;
    }

    bool Flag(const char* key, bool fallback = false) const
    {
        const PsuCapability* c = Find(key);
        return c ? c->AsBool() : fallback;
    }

    /// Write by key. IllegalDataAddress when this supply does not have the
    /// capability at all — which is how a supply with no backlight answers.
    ModbusError Write(const char* key, float value)
    {
        PsuCapability* c = Find(key);
        if (c == nullptr)
            return ModbusError::IllegalDataAddress;
        if (!c->Writable())
            return ModbusError::WritingNotAllowed;
        return Write(*c, value);
    }

    PsuCapabilityIterator begin() const { return PsuCapabilityIterator(head_); }
    PsuCapabilityIterator end() const { return PsuCapabilityIterator(nullptr); }

protected:
    Psu() = default;

    /// Called from the driver's constructor, once per capability, in declaration
    /// order. Appends at the tail so display order follows the driver's table.
    void Add(PsuCapability& c)
    {
        if (c.registered)
            FATAL("capability '%s' registered twice", c.key);
        c.registered = true;
        c.next = nullptr;
        if (tail_ == nullptr)
            head_ = tail_ = &c;
        else
        {
            tail_->next = &c;
            tail_ = &c;
        }
    }

private:
    PsuCapability* head_ = nullptr;
    PsuCapability* tail_ = nullptr;
};

// ──────────────────────────────────────────────────────────────
// Well-known capability keys: the semantic vocabulary the PRODUCT knows by
// meaning rather than by iteration — the chart plots one, the setpoint controls
// write three, the log line prints five. Everything else the application only
// ever sees as a row in the chain.
//
// A driver is free to offer none of these (nothing crashes; a write refuses what
// is absent), but a supply that HAS the concept must use the spelling here, or
// the dashboard will not find it. These are capability names, not register
// names — no supply's map appears in this list.
// ──────────────────────────────────────────────────────────────
namespace PsuKey
{
    constexpr const char* SetVoltage         = "setVoltage";
    constexpr const char* SetCurrent         = "setCurrent";
    constexpr const char* OutputVoltage      = "outputVoltage";
    constexpr const char* OutputCurrent      = "outputCurrent";
    constexpr const char* OutputPower        = "outputPower";
    constexpr const char* InputVoltage       = "inputVoltage";
    constexpr const char* OutputEnabled      = "outputEnabled";
    constexpr const char* KeyLock            = "keyLock";
    constexpr const char* ConstantCurrent    = "constantCurrent";
    constexpr const char* Backlight          = "backlight";
    constexpr const char* ProtectionState    = "protectionState";
    constexpr const char* RuntimeSeconds     = "runtimeSeconds";
    constexpr const char* ChargeAh           = "chargeAh";
    constexpr const char* EnergyWh           = "energyWh";
    constexpr const char* Temperature        = "temperature";
    constexpr const char* ExternalTemperature = "externalTemperature";
    constexpr const char* ActivePreset       = "activePreset";
}
