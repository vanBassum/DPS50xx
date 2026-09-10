# Next up

**Active work only.** Rewritten constantly, kept tiny, and an item is *removed* when it
lands or is dropped — never ticked off in place. Everything else lives in
`docs/backlog/` (work for later) or `docs/reasoning/` (why things are the way they are).
If a fact wants to survive, it does not belong in this file.

Last updated 2026-09-10.

## Now

**The frontend is one SPA again, and modules are off.** The shell/module split was
dropped on 2026-09-10: it cost ~2,000 lines of seam and bought this product nothing, and
the relay serves a device's whole page just as happily. `ui.modules` (default false here,
true upstream) gates the manifest in `UiManager`, so nothing in `strux/` stopped
registering and turning it back on is a setting rather than a revert. See
`reasoning/2026-09-10-17h24`. Baseline was `e1ca108`; the capability model, the shadcn b0
preset, `DeviceInfoDialog`, `uploadSession`/`downloadSession` and `partition status` all
came forward.

**Not verified in a browser.** Both commits are a build and a typecheck only. What wants
looking at on real hardware: the two-column dashboard at `lg` and collapsed below it, the
charts in BOTH themes (their structural colours are theme variables, and dark has never
been exercised on this page), and the same page served through the relay — that last one
is the path the empty manifest actually changes.

**Recharts is back at 3.10.1**, against code written for 2.15.4. It typechecks and the API
it uses is unchanged across that major, but no chart has been rendered yet. If something
looks wrong before anything else is suspected, pin `^2.15.4` and compare.

**One hazard worth knowing before the next board change:**

- **The board is a build-time choice with no runtime witness.** Flashing the XY6020L with
  `-DBOARD=dps50xx_c3` builds, boots, joins the network and serves the UI — and reports the
  supply offline, indistinguishable from a wiring fault, because Modbus moves from 3/4 at
  115200 to 6/5 at 9600. Adding `board` to `system info` would make a mis-flash obvious;
  the compile already defines `BOARD_XY6020_C3`. Not done.

**The XY6020L is flashed from `sdkconfig-xy6020` / `build-xy6020`.** The checked-out root
`sdkconfig` is stale for it — it is esp32c3 but with USB Serial/JTAG as the PRIMARY console,
which the board defaults override and `ConsoleManager.cpp` refuses to build. Use the
per-board pattern: `idf.py -DBOARD=xy6020_c3 -DSDKCONFIG=sdkconfig-xy6020 -B build-xy6020`.

**The device shell is on shadcn preset `b0`.** It never had been before 2026-09-10, which
is where the typeface and radius divergence came from; see `reasoning/2026-09-10-13h20`.
The relay's token set no longer has to match, since the relay now serves this page whole
rather than hosting modules inside its own chrome.

Two things it left:

- **Inter costs 213 KB of `www` and 166 KB of that can never be served.** Seven
  `unicode-range`-gated subsets ship; only latin (47 KB) is ever requested by an English
  UI, but flash holds all of them. The prune is one import away and was NOT taken, because
  the next `shadcn apply` would silently undo it. Take it if the partition gets tight.
- **The preset upgraded every shadcn component**, which brought `cn` (shadcn's own
  clsx+tailwind-merge replacement) and `next-themes` into the shell. `next-themes` is only
  there because the current `sonner.tsx` imports `useTheme`, and this shell has no theme
  provider — `useTheme()` outside one returns undefined and the default `"system"` applies,
  so it works, but the dependency earns nothing.

**The XY6020L runs on hardware, on the capability model, updated over the air.** Flashed to the
C3 at `E8:3D:C1:9C:1C:CC`, then twice updated by OTA with no cable attached. What the unit
confirmed, in `psu get`:

- `runtimeSeconds` 4731 (01:18:51) folded from three registers; those three appear nowhere.
- `externalTemperature` reports **unavailable** — the 8888 sentinel fired, and 888.8 °C is gone.
- `chargeAh` 0.67 Ah and `energyWh` 2.23 Wh, so the 32-bit low-word-first decode is right.
- `activePreset` = `M0` with ten options; `protectionState` = `None` with eleven.
- **The protection block answers.** LVP 6.0 V, OVP 62.0 V, OCP 22.0 A, OTP 95 °C — and OTP
  matching the vendor's documented default is what confirms that block's per-unit scales.
- `psu write` writes, and refuses a read-only capability, an out-of-range value (against the
  capability's own max) and an unknown key.

**Two scale questions left, one of them still needing a load.**

1. **The live power scale (0x0004).** Output was on at 4 V into nothing, so power read 0.00 W.
   The 0.1 W choice is still only the range argument in `reasoning/2026-09-09-18h52`. Put a load
   on and compare with the panel: 10× low means 0.01 W.
2. **OPP is now 1 W per count, inferred not measured.** At 0.1 W this unit read an odd 120 W; at
   1 W it reads 1200 W, exactly its rating, and the vendor's documented 950 W default would be an
   implausible 95 W at 0.1. The neighbouring OTP confirms the block uses natural units where the
   live block uses hundredths. One look at the panel's OPP setting settles it.

**`protOverCurrent` came back holding 22.0 A on a 20 A supply**, so the declared max is 25 A —
the rating is not the register's ceiling, and refusing to re-enter a value the panel already
accepted would repeat the `MAX_CURRENT` mistake. Same open question as the DPS's 20.1 A.

**`model` still reads 25858 and `version` 140.** Both answer every poll; neither is known to
mean what it is named. They are in the `info` group, so nothing displays them.

**The M0 caveat.** The protections are read from memory group M0's block (0x0052–0x005D). The
addresses are per-group, so this is correct only while the panel is on M0 — which is what
`activePreset` reports today. Reading the active group's block would mean addressing the poll off
another capability's value, which nothing supports.

**Preset recall and the composite limits are read-only on purpose.** Recalling a preset rewrites
every setpoint at once, and max charge / max energy / max runtime are 32-bit or split-register
values that would need a multi-register write `ModbusPsu` does not do. The UI shows them and does
not pretend to set them.

**Synced with Strux on 2026-09-09, and the debt list shrank because upstream took
most of it.** `strux/` and `lib/` are now Strux `f7e0501` plus the deltas below;
`docs/reasoning/2026-09-09-23h20` is why this was a merge rather than the copy CLAUDE.md
describes.

What is STILL fork-local in `strux/` and `lib/`, all of it in one area plus one file:

- **A second WiFi network** (`wifi.ssid2`/`wifi.password2`, the rotation, `SwitchSta`,
  `CurrentSsid`, `DescribeRound`). Upstream cycles STA rounds with AP windows exactly as
  this fork does, but against ONE network. Still unverified with two actually configured.
- **RSSI on a LinkDown** (`NetworkEvent::rssi`, `BeaconStrength`) — what tells an AP
  refusing us from an AP too far away, see `reasoning/2026-08-26-21h22`.
- **A scan that reports `authmode`, not just `secure`** (`AuthModeName`) — WPA2/WPA3
  transition mode needs PMF and refuses a station without it, which looks exactly like
  being out of range. See `reasoning/2026-08-26-21h19`+.
- **`lib/protocol`'s `ArgType::Float`**, still four lines across three files, still the
  thing that silently breaks `psu set` if `lib/` is copied naively.

Upstream has since taken, so these are no longer fork edits: the disconnect-reason code
in `NetworkEvent`, the AP-window cycle, `HasUpstream()`, PMF advertisement, the
`writePartition` envelope fix, and the blocking-console guard.

**DFS is off in this fork, and the reason is worth reporting upstream.** Upstream's
defaults enable `CONFIG_PM_ENABLE`/`CONFIG_PM_DFS_INIT_AUTO` and price it at "a few
microseconds of extra interrupt latency". On a C3 serving this UI it cost `system ping`
3925 ms against 72 ms, and produced 45 % ICMP loss that looked for hours like a marginal
radio — the CPU could not drain its RX path, so packets were dropped and TCP paid
retransmit backoff. See `reasoning/2026-09-10-00h40`; the pacing test in it is the cheap
way to tell a starved chip from a weak link.

**One upstream bug found while syncing, and it is still upstream's.**
`SystemManager::GetDeviceName()` falls back to the build's project name when the stored
name is empty and can never reach it, because `device.name`'s typed default is the
literal `"Strux"`. Every fork therefore ships a device called Strux — in the tab title,
the DHCP hostname and the relay's device list. A `""` default is the whole fix. Worked
around here per device: both units are named (`XY6020L`, `DPS5020`).

**`frontend/src/config.ts`'s `DEV_HOST` is stale.** It says `dps50xx.local`, and no
device answers to that any more — mDNS follows `device.name`, so the two units are
`xy6020l.local` and `dps5020.local`. Only `pnpm dev` reads it. Part of the unresolved
rename question below.

**The AP→STA half of the WiFi cycle is unverified on hardware.** Three attempts then a
15-minute AP window is confirmed on the ESP32 — a round now takes 15 s rather than 30,
since a failed attempt is retried on the failure instead of on the timeout. The window
*closing* and the round that follows it are still unverified: the device has never been
left alone for the full fifteen minutes.

**A second WiFi network can be configured, and has never been tried with two.**
`wifi.ssid2`/`wifi.password2` are registered and the round rotates between the
configured networks, three attempts each. With one configured the code takes the same
path it always did, which is what the last flash verified; the rotation itself, and
`SwitchSta` changing networks on a running station, have only ever run as a one-network
degenerate case. Configuring the fallback needs a device you can reach — the settings UI
over `Strux-AP` during a recovery window.

**`vanBassumExt` is out of range of the bench, not wedged.** Two attempts in three
cannot see it at all (reason 201) and the third hears it at −83 dBm — see
`reasoning/2026-08-12-14h05`, which revises the wedged-extender inference in
`2026-08-10-19h19`. Point `wifi.ssid` at the main router or move the device; there is no
firmware fix, and every WiFi log from this bench will keep showing recovery AP windows
until one of those happens.

**`web.password` is empty, and the AP now recurs by design.** Every outage window puts an
unauthenticated console on an open network, which mattered less when the AP was a
one-way trip. Either set a default or make the recovery AP carry the web password.

**The DPS5020 is intermittent, and the bench is where to look.** It alternates between
answering completely and being entirely absent for minutes — a stray
`RX buffer had 31 stale bytes` is exactly one whole 13-register response, and the
instrumented driver then measured 48 consecutive polls receiving literally zero bytes.
Baud, wiring, address and register map are therefore all right; see
`reasoning/2026-08-11-22h33`. Suspect the supply or a connector, not the firmware.
Nothing about the product is verifiable until it stays up.

**The C3 board has never been flashed.** `dps50xx_c3` compiles (1.24 MB, 21% free) and
nothing in it is board-specific beyond `BoardConfig.h`, but the pins are unverified
against real hardware.

**`MAX_CURRENT` is 20.0 A and the hardware accepts more.** The test unit had 20.1 A set
from its front panel, so the firmware now refuses to reproduce a setpoint the panel
allows. Either the cap is the product's rating and the asymmetry is intended, or it
should follow what the register accepts. Not decided.

**The `www` partition is 0xE0000 (917 KB) and the bundle is 482 KB** — 52.6% used,
425 KB free. Recharts is 112 KB of that and Inter 213 KB, so the two obvious growth
sources are both already spent.
