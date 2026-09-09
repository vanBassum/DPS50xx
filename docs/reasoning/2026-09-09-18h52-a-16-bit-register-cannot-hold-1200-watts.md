---
id: 2026-09-09-18h52
date: 2026-09-09
time: "18:52"
title: A 16-bit register cannot hold 1200 watts
builds-on: 2026-09-09-18h32
supersedes:
---

**Before:** the XY6020L's register map had to come from somewhere, and the assumption was that
"from the datasheet" and "from a library" were a quality ordering — use the vendor document,
fall back to code only if the document is unobtainable. The vendor's Modbus interface PDF exists
and is public.

**What changed it:** the PDF is not extractable here (its text is CID-encoded, no poppler on this
machine, and the mirror that renders it as HTML answers 403), while a maintained Arduino library
publishes the same map as named constants with scales in the comments. So the fallback became the
primary source — and then the fallback contradicted *itself*: in
`Jens3382/xy6020l/src/xy6020l.h`, the register comment for 0x0004 says `0,1 W` and the doxygen on
the accessor that reads the very same register says `LSB: 0.01 W`.

No third source settles it, and the bench is not here. What settles it is arithmetic: this is a
1200 W supply, an unsigned 16-bit register holds 65535, and 0.01 W per count tops out at
655.35 W. **0.01 W cannot express the supply's own rating**, so the scale is 0.1 W. That is not a
preference between two sources; it is one of them being impossible.

The general shape is worth keeping, because a register map is mostly numbers with no internal
redundancy — but not entirely. A scale, a width and a rated maximum are three facts that
constrain each other, so a claimed scale can be checked against the label on the box before any
hardware is powered. Two more of this map's claims were checked the same way and *cannot* be:
the 32-bit word order (low word at the lower address, per the library's own naming) and the
eleven protection codes both have to be believed until a unit answers.

**Now:** `XY6020L.h` carries 0.1 W with the range argument written next to it, and the whole map
is marked bench-unverified in the header rather than presented as fact. A wrong scale here reads
10× low, which the first real poll makes obvious — so the cost of being wrong is a one-line fix,
and the cost of *pretending* to be sure is someone later trusting it.

**Follows:** the driver names its provenance (a reverse-engineered library, credited to
g-radmac, plus the vendor document for the protection codes) instead of implying a datasheet; and
`docs/next-up.md` lists what the first flash must check, in the order that a wrong answer would
be visible — power scale, word order, protection codes, then the TX/RX orientation that produces
no answer at all rather than a wrong one.
