# Remote access / relay server — what is left

Devices connect **outbound** to a server (NAT-friendly) so they are reachable
remotely. The relay is just another dumb-pipe transport into `CommandManager`, which
is why every command — including firmware update — works remotely for free. That is
the driving reason `CommandManager` is the star point of the architecture.

Demo server: [`relay-server/`](../../relay-server/) — Python, deliberately, to prove
the path before writing an ASP.NET one, and the device side assumes nothing about the
server's language.

**Being deployed step by step in
[`2026-08-05-relay-in-production.md`](2026-08-05-relay-in-production.md)** — that plan
covers items 1, 2 and 3 below (credential, TLS, and the browser side, the last of which
Authentik answers), so work from it rather than from this list.

## What is already proven on hardware

esp32_devkit against `relay.py` on a LAN host, 2026-08-02 and 2026-08-05:

- The device dials out, the dashboard lists it, clicking it opens the device's *own* web
  UI through the command pipe, and commands plus live log broadcasts work remotely.
- A 413 KB frontend bundle arrives byte-identical to the device-served copy, gzip
  `Content-Encoding` passed straight through. A missing asset 404s; an unknown route
  falls back to `index.html`.
- **Firmware update end to end**: 1,120,064 bytes to the idle OTA slot, `activate`
  validating the image, and boot into the new slot. 5.5 s unpaced (199 KB/s); a run
  paced to 40 s — twice the old watchdog budget — survived a page load cutting in at
  t=24.5 s, which used to kill both requests.
- One later build was pushed over the relay with no serial cable attached.
- Liveness: recovery from failed connects, and 100 s fully idle without losing the pipe.

Not proven: anything with more than one browser or one operator.

## `wss://` through the public relay serves a broken bundle (2026-09-09)

Tried for the first time — `wss://strux.vanbassum.com/device`, the deployed server behind an
Authentik proxy, rather than `relay.py` on the LAN over `ws://`. The device connects and is
listed, but opening `/devices/esp32-e83dc19c1ccc/` renders a blank page with one console error:

    Uncaught SyntaxError: Unexpected token 'var'    index-kyco_E2K.js:8

**The device side is exonerated, byte for byte, over the relay's own command path.** Pulling the
same asset with `web read` (473 chunks) returns 241,889 bytes whose sha256 equals the build
output, with a header declaring `contentType: application/javascript` and
`contentEncoding: gzip`; the device's HTTP server returns the identical bytes, and the
decompressed bundle passes `node --check`. So the bytes leaving the device are correct and the
corruption is downstream — the relay, its cache, or the proxy in front of it.

Why the error points at a transform rather than a truncation: line 8 of that bundle opens with
`` `+f.stack}}var gt= ``, the closing half of a template literal whose newline is INSIDE the
backticks (line 7 opens it). A parser reaching `var` in a bad state there means the preceding
bytes were damaged, not that the file ended early — and the first seven lines parsed, so it is
not "gzip served as text" either.

Next diagnostic, which needs a browser logged into the SSO: in DevTools → Network, compare that
file's transferred size against **241,889** (correct, gzip) and check whether
`Content-Encoding: gzip` survived the proxy. Also whether the CSS (20 KB, far fewer chunks)
arrives intact — if only the 473-chunk JS is damaged, the bug is size- or chunk-dependent.

One asymmetry found while testing, worth knowing on both sides: **`web read` refuses a leading
slash.** `assets/index-kyco_E2K.js` returns 200; `/assets/index-kyco_E2K.js` returns
`{"status":404}`, as does `/index.html`. A relay that passes a URL path straight through gets a
404 for everything, so the working one must be stripping it — fine, but it is an undocumented
contract between two repositories and nothing on the device side declares it.

## Open, in the order that will probably matter

1. **The device→server credential.** The worst gap by far: anything that guesses the
   MAC-derived device id does not impersonate the device, it **evicts** it — the
   server keys its registry on that id and the newcomer takes the slot. Until this
   exists the relay must not face a public IP. TLS, server-side login and a persistent
   registry all sit behind the same question: *does this ever face a public network,
   or is it a LAN convenience?* Answer that and the rest follows.

2. **TLS / `wss://` — untested, not unimplemented.** `RelaySocket` attaches the
   certificate bundle when the URL is `wss://`, and the build has the bundle enabled,
   but no run has ever used it. Two things to check when someone does: that the
   handshake succeeds at all, and that it fits the relay task's stack — 10 K, sized by
   reasoning rather than measurement, and now shared with the command handlers because
   one task does both. If it is tight, measure with `uxTaskGetStackHighWaterMark`
   rather than guessing again.

3. **Per-browser auth on a shared pipe.** The pipe is *one* connection, so its auth
   state is shared: a login by one remote user authenticates the pipe for every
   browser the server relays. Acceptable for a single trusted operator, wrong for
   anything else. Needs the server to carry a client identity alongside the session
   id — which is a protocol change, not a device change.

4. **Server-side file caching**, keyed on `(deviceId, firmware, path)`. An uncached
   page load is N sequential WAN round trips, because one request in flight per device
   is now permanent (multiplexing was rejected 2026-08-03 — concurrency is not worth a
   slot table and per-channel buffers on this much RAM). Caching is the fix for page
   load latency; the slow fetch-through-the-pipe then happens once per firmware
   version instead of once per view. Version-keyed so a device-served UI can never
   mismatch its firmware, which is the whole reason the UI is served from the device
   rather than copied onto the server.

5. **A legitimately silent command longer than the gate's idle window.**
   `partition clear` says nothing while it erases: 4.1–4.5 s measured for a 1.5 MB
   slot, against a 15 s window. Fine today, but the margin is the erase time of the
   largest partition rather than a number anyone chose. The fix, if a bigger partition
   ever needs it, is for the handler to report progress the way `partition write`
   already does — not a longer timeout.

## Settled, so nobody reopens them

- **`getWebFile` is `httpGet`-flavoured** (status + content-type + content-encoding +
  body), not raw bytes: frontend files are stored gzipped, so raw bytes would force
  the server to re-derive `Content-Encoding` and MIME, where they drift from the
  device.
- **SPA fallback belongs to each route layer**, not to the device's `Resolve()`, so a
  mistyped asset path 404s instead of returning `index.html` with a 200 and tripping a
  browser MIME error.
- **The server is payload-opaque, not frame-opaque.** It must read and rewrite the
  3-byte session header because it owns the id space on the device pipe, and it must
  forward session 0, so the relay is not request/response.
- **WireGuard was considered and rejected** — a network-level fix for an app-level
  problem, with heavy key and routing operations.
