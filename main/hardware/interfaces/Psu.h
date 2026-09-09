#pragma once

#include "Fatal.h"
#include "ModbusError.h"
#include <cstdint>
#include <cstring>

// ──────────────────────────────────────────────────────────────
// Role interface: application vocabulary for "the bench supply on the wire",
// and the intrusive chain of readings that describes it.
//
// The point of the chain — and the reason this is not a struct of named floats
// like DPS5020Data was — is that a DRIVER IS THE ONLY THING THAT KNOWS A
// REGISTER MAP. `psu get`, the telemetry point and the dashboard are walks over
// whatever a driver registered, so a supply that reads more (an XY6020L's energy
// counters, its input temperature, its output timer) shows up in all three
// without a line changing above the driver.
//
// Same registration pattern as Setting and CommandEntry: the owner provides the
// memory, the entry IS the link, misuse fails on the first boot. Two deliberate
// differences from Setting:
//
//   • Readings are ORDINARY MEMBERS of the driver, not `inline static`. A
//     setting is static because there is one settings namespace per device;
//     reading VALUES are per instance, and two supplies on one bus sharing
//     static value storage would be a silent, ugly bug. Do not "fix" this to
//     match Setting.
//   • The chain appends at the tail, where Setting's pushes at the head.
//     Declaration order is register order is display order, and a dashboard
//     that reordered itself on every firmware change would be worse than the
//     one extra pointer costs.
//
// ModbusError appears here, which looks like bus vocabulary leaking into a role.
// It is from `lib/` — layer-free substrate, like Stream — and a `psu set` reply
// says WHICH error it was, so flattening it to a bool would lose the one thing
// the caller can act on.
// ──────────────────────────────────────────────────────────────

class Psu;
class PsuReadingIterator;

/// How to render and interpret `value`. A generic consumer needs this and
/// nothing else — no downcasts, no per-type accessors.
enum class PsuKind : uint8_t
{
    Number,     ///< value * unit, e.g. 12.48 V
    Bool,       ///< 0 or 1
    Enum,       ///< a register code with a label table (protection state)
};

enum class PsuAccess : uint8_t
{
    Read,
    ReadWrite,
};

// ──────────────────────────────────────────────────────────────
// One reading: the chain link plus the schema facts that describe it to
// everything above. Deliberately carries NO Modbus address — that is the
// driver's business (see PsuRegister in drivers/ModbusPsu.h), and nothing in
// the application or the UI has ever wanted one.
// ──────────────────────────────────────────────────────────────
struct PsuReading
{
    const char* const key;      ///< wire field name — stable, this is API
    const char* const label;    ///< shown in the UI
    const char* const unit;     ///< "" where there is none
    const char* const telemKey; ///< telemetry field name; nullptr = not recorded

    const PsuKind   kind;
    const PsuAccess access;

    /// Writable range, in the same units as `value`. Meaningless when access is
    /// Read. This is where MAX_VOLTAGE/MAX_CURRENT went: the limit belongs to
    /// the supply that has it, and it travels to the UI in `psu get` so the
    /// browser stops hardcoding 50 and 20.
    const float min;
    const float max;

    const char* const* const enumLabels;  ///< Enum only, else nullptr
    const uint8_t            enumCount;

    /// The decoded value, rewritten by the driver on every poll. One field
    /// serves every kind — a bool is 0/1 and an enum is its register code — so
    /// a generic consumer never has to ask what it is holding.
    float value = 0;

    bool AsBool() const { return value != 0.0f; }
    bool Writable() const { return access == PsuAccess::ReadWrite; }
    bool InRange(float v) const { return v >= min && v <= max; }

    /// The enum's label for the value it currently holds, or nullptr for every
    /// other kind. This is what killed PROTECTION_LABELS in the frontend: the
    /// supply that defines the codes is the thing that names them.
    const char* ValueLabel() const
    {
        if (kind != PsuKind::Enum || enumLabels == nullptr)
            return nullptr;
        const auto i = static_cast<uint8_t>(value);
        return i < enumCount ? enumLabels[i] : "?";
    }

    PsuReading(const PsuReading&) = delete;
    PsuReading& operator=(const PsuReading&) = delete;

    /// A registered reading is a live chain link; letting it die would leave a
    /// dangling pointer behind. Same reasoning as ~Setting and ~CommandEntry.
    virtual ~PsuReading()
    {
        if (registered)
            FATAL("registered reading '%s' destroyed — a supply's register table "
                  "must live as long as its driver", key);
    }

protected:
    PsuReading(const char* key, const char* label, const char* unit, const char* telemKey,
               PsuKind kind, PsuAccess access, float min, float max,
               const char* const* enumLabels, uint8_t enumCount)
        : key(key), label(label), unit(unit), telemKey(telemKey),
          kind(kind), access(access), min(min), max(max),
          enumLabels(enumLabels), enumCount(enumCount) {}

private:
    friend class Psu;                  // links the chain, decodes into `value`
    friend class PsuReadingIterator;   // walks it

    PsuReading* next = nullptr;
    bool registered = false;
};

// ──────────────────────────────────────────────────────────────
// Iteration — the only public way to walk the chain, which is what lets `next`
// stay private. Minimal forward iterator, enough for `for (PsuReading& r : psu)`.
// ──────────────────────────────────────────────────────────────
class PsuReadingIterator
{
    PsuReading* cur_;

public:
    explicit PsuReadingIterator(PsuReading* r) : cur_(r) {}

    PsuReading& operator*() const { return *cur_; }
    PsuReadingIterator& operator++() { cur_ = cur_->next; return *this; }
    bool operator!=(const PsuReadingIterator& o) const { return cur_ != o.cur_; }
    bool operator==(const PsuReadingIterator& o) const { return cur_ == o.cur_; }
};

// ──────────────────────────────────────────────────────────────
// The role. Owns the chain (so PsuReading's link can stay private) and leaves
// the wire to the implementation — see ModbusPsu, which is where retries,
// batching and the offline threshold live.
// ──────────────────────────────────────────────────────────────
class Psu
{
public:
    virtual ~Psu() = default;

    Psu(const Psu&) = delete;
    Psu& operator=(const Psu&) = delete;
    Psu(Psu&&) = delete;
    Psu& operator=(Psu&&) = delete;

    /// Read every registered reading. The error is returned for logging; whether
    /// the supply is CONSIDERED gone is IsOnline()'s business, because one missed
    /// transaction is normal on a supply whose MCU also drives its front panel.
    virtual ModbusError Poll() = 0;

    virtual bool IsOnline() const = 0;

    /// Write one reading, in its own units. The implementation scales it, sends
    /// it, and updates `value` only if the supply took it.
    virtual ModbusError Write(PsuReading& reading, float value) = 0;

    // ── Lookup by key ─────────────────────────────────────────
    // A short chain walk with strcmp, which is free at poll rates. A hot
    // consumer should hold the pointer Find() returns rather than looking the
    // same key up in a loop.

    PsuReading* Find(const char* key) const
    {
        for (PsuReading* r = head_; r != nullptr; r = r->next)
            if (std::strcmp(r->key, key) == 0)
                return r;
        return nullptr;
    }

    bool Has(const char* key) const { return Find(key) != nullptr; }

    float Number(const char* key, float fallback = 0.0f) const
    {
        const PsuReading* r = Find(key);
        return r ? r->value : fallback;
    }

    bool Flag(const char* key, bool fallback = false) const
    {
        const PsuReading* r = Find(key);
        return r ? r->AsBool() : fallback;
    }

    /// Write by key. IllegalDataAddress when this supply has no such
    /// register — which is exactly how a supply without a backlight answers.
    ModbusError Write(const char* key, float value)
    {
        PsuReading* r = Find(key);
        if (r == nullptr)
            return ModbusError::IllegalDataAddress;
        if (!r->Writable())
            return ModbusError::WritingNotAllowed;
        return Write(*r, value);
    }

    PsuReadingIterator begin() const { return PsuReadingIterator(head_); }
    PsuReadingIterator end() const { return PsuReadingIterator(nullptr); }

protected:
    Psu() = default;

    /// Called from the driver's constructor, once per reading, in declaration
    /// order. Appends at the tail so display order follows the register map.
    void Add(PsuReading& r)
    {
        if (r.registered)
            FATAL("reading '%s' registered twice", r.key);
        r.registered = true;
        r.next = nullptr;
        if (tail_ == nullptr)
            head_ = tail_ = &r;
        else
        {
            tail_->next = &r;
            tail_ = &r;
        }
    }

private:
    PsuReading* head_ = nullptr;
    PsuReading* tail_ = nullptr;
};

// ──────────────────────────────────────────────────────────────
// Well-known keys: the handful of readings the PRODUCT knows by meaning rather
// than by iteration — the chart plots one, the setpoint controls write three,
// the log line prints five. Everything else the application only ever sees as a
// row in the chain.
//
// A driver is free to register none of these (nothing crashes; `psu set` refuses
// what is absent), but a supply that has the concept must use the spelling here
// or the dashboard will not find it.
// ──────────────────────────────────────────────────────────────
namespace PsuKey
{
    constexpr const char* SetVoltage      = "setVoltage";
    constexpr const char* SetCurrent      = "setCurrent";
    constexpr const char* OutVoltage      = "outVoltage";
    constexpr const char* OutCurrent      = "outCurrent";
    constexpr const char* OutPower        = "outPower";
    constexpr const char* InVoltage       = "inVoltage";
    constexpr const char* OutputOn        = "outputOn";
    constexpr const char* KeyLock         = "keyLock";
    constexpr const char* ConstantCurrent = "constantCurrent";
    constexpr const char* Backlight       = "backlight";
    constexpr const char* Protection      = "protection";
}
