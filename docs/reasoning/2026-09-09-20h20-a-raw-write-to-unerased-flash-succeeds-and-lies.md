---
id: 2026-09-09-20h20
date: 2026-09-09
time: "20:20"
title: A raw write to unerased flash succeeds and lies
builds-on: 2026-09-09-18h32
supersedes:
---

**Before:** the first OTA of the session worked on the first try — `partition write ota_1`, then
`partition activate ota_1`, then reboot, and the device came up on the new slot. The obvious
lesson to draw was "the OTA path works", and the sequence I had just used looked like the whole
procedure.

**What changed it:** the second OTA, twenty minutes later, with the same script and a same-sized
image. `partition write ota_0` reported `{"ok":true,"size":1258400}` — every byte accounted
for — and then `activate` refused:

```
E esp_image: invalid segment length 0x40432022
E PartitionWriter: set_boot_partition 'ota_0': ESP_ERR_OTA_VALIDATE_FAILED
```

`0x40432022` is an IRAM address, not a length: the image reader was walking bytes that were not
the image it thought it was reading. `partition write` is a RAW write (that is the documented
design — data partitions are raw erase+write, and this path does not go through `esp_ota_write`),
and **flash cannot be written without erasing**. Writing over an existing image ANDs the new bits
into the old ones. Segment 0 mapped fine because the two images share their first pages
byte-for-byte; the corruption starts where they first differ.

So the first write did not demonstrate the procedure — it demonstrated a **blank partition**.
`idf.py flash` had only ever written `ota_0`, leaving `ota_1` erased, and an erased page accepts
any write faithfully. The sequence that worked was incomplete and the reason it worked was
invisible in its own success.

`PartitionWriter::Activate` even names the right sequence in a comment — "keeps the caller's
sequence uniform (clear → write → activate)" — and `partition clear` exists for exactly this. I
had read neither closely, because the first attempt had already "worked".

The same mistake then repeated on the `www` partition, and there the failure was quieter: the FAT
image mounted, the HTTP server served, and `GET /` returned 314 bytes of directory bytes with no
error anywhere. A corrupt filesystem image is not refused by anything — no validator sits between
a FAT partition and a browser the way `esp_image` sits in front of an app. It cost a second round
of confusion to notice that the same missing erase explained both.

**Now:** OTA over the wire is **clear → write → activate → reboot**, for app slots and for `www`
alike, and it is in CLAUDE.md as a procedure rather than living in whoever last did it. The erase
of a 1.5 MB slot takes a few seconds and returns nothing until it is done.

**Follows:** the app image at least fails loudly, which is what `esp_image` validation is for, and
what makes the app half of a botched update recoverable rather than a brick — the boot pointer
stays where it was. The data half has no such guard, so a `www` write is the one that deserves a
read-back check afterwards: fetch an asset and compare it against the build output, which is now
how the frontend delivery is verified.
