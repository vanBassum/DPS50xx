#pragma once

#include "interfaces/Led.h"
#include "interfaces/Psu.h"

// ──────────────────────────────────────────────────────────────
// The board layer's provider: the ROLES every board owes, in application
// vocabulary. `interfaces/` holds the individual roles; this states which of
// them a board must bind.
//
// The bottom layer's half of the same context/provider pair the two layers
// above use — BoardContext owns the driver instances and answers this.
//
// Deliberately roles ONLY. A concrete driver accessor (the escape hatch, for
// when the application needs a driver's full API) stays off this interface and
// on BoardContext itself, where it is checked at compile time. That is what
// keeps this list from becoming the union of every board's peripherals: the day
// one board grows a display, no other board owes a MockDisplay.
//
// A board without the hardware for a role binds a Mock* driver — MockLed and
// friends exist for exactly that, so satisfying every role is the existing
// discipline and not a new tax. What this adds is where the failure lands: a
// board that forgets a role now fails in the board, saying it left a pure
// virtual unimplemented, instead of failing later at some call site in
// application code.
//
// Multi-instance roles get a semantic enum parameter (Sensor::Ambient, never
// Sensor_2) — introduce it with the first multi-instance role.
// ──────────────────────────────────────────────────────────────

class BoardProvider
{
public:
    virtual ~BoardProvider() = default;

    virtual Led& GetLed() = 0;

    // The supply. This was deliberately NOT a role while there was one of them:
    // a `Psu` interface would have been "DPS5020's whole API or a lossy subset
    // of it", and every board would have owed a MockPsu for it.
    //
    // A second supply inverts that argument. What the two share is not one
    // chip's API but a register CHAIN plus four verbs over it, and that is the
    // application's own vocabulary rather than either supply's — see
    // interfaces/Psu.h. MockPsu exists for a board with nothing on the wire, the
    // same way MockLed does.
    //
    // BoardContext::GetDps() stays alongside it as the concrete escape hatch,
    // for the day something needs a DPS5020's own API.
    virtual Psu& GetPsu() = 0;
};
