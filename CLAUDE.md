# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

DPS50xx is ESP32 firmware (ESP-IDF v6.0, C++, FreeRTOS) that drives a DPS5020 bench power supply over Modbus RTU, with a React web UI. It is a fork of [Strux](https://github.com/vanBassum/Strux), which supplies everything that is not the supply: WiFi, settings, OTA, the log console, authentication, relay-based remote access and telemetry.

**The product is three files' worth of code.** Everything else is framework:

- [main/app/PsuManager/](main/app/PsuManager/) — polls the supply, owns `psu get` / `psu set`, registers `psu.poll` and `psu.telem`, records a telemetry point per poll.
- [main/hardware/drivers/DPS5020.h](main/hardware/drivers/DPS5020.h) — the register map and the retry/offline logic.
- [main/hardware/boards/](main/hardware/boards/) — `dps50xx_esp32` and `dps50xx_c3`, each owning its pins and binding the drivers.
- [frontend/src/pages/HomePage.tsx](frontend/src/pages/HomePage.tsx) + [frontend/src/hooks/use-psu.ts](frontend/src/hooks/use-psu.ts) — the dashboard.

Nothing in `strux/` is touched by any of that, which is the point: see the "Pulling from Strux" section below.

## Build commands

Firmware (requires ESP-IDF v6.0+ environment):

```bash
idf.py set-target esp32
idf.py build                          # also builds the frontend if pnpm is installed
idf.py -p <PORT> flash monitor
```

**The ESP32 test unit does not enter download mode on its own** — RTS resets the chip but
DTR never reaches IO0, so `flash` fails with `Wrong boot mode detected (0x13)` no matter
how the reset is timed. Hold the BOOT button while flashing, and pass
`--connect-attempts 0` so esptool keeps retrying the reset until the button is down:

```bash
python -m esptool --chip esp32 -p <PORT> -b 460800 --connect-attempts 0 \
    write-flash "@flash_args"     # run from build/
```

The board is a build-time choice; pass it to *every* `idf.py` invocation, including `set-target`, because the board folder is what carries `CONFIG_IDF_TARGET`:

```bash
idf.py -DBOARD=dps50xx_c3 set-target esp32c3
idf.py -DBOARD=dps50xx_c3 build
```

Boards live in `main/hardware/boards/`; the default is `dps50xx_esp32`. Building a second board into the same `build/` directory will reconfigure it — use `-B build-c3` to keep both. Note that `-DSDKCONFIG=` with a dot in the filename is parsed as a build target and fails; use a dash.

Frontend (React 19 + TypeScript + Vite + Tailwind + shadcn/ui, package manager is pnpm):

```bash
cd frontend
pnpm dev          # hot-reload dev server, proxies WebSocket to a running device
pnpm build        # tsc -b && vite build && gzip into ../www (embedded in flash as FAT image)
pnpm typecheck    # tsc --noEmit
```

There are no automated tests; verification is building, flashing, and driving the device over its own wire:

1. `idf.py build`, `idf.py -p <PORT> flash` — find the port, don't trust a number in a doc.
2. Open a WebSocket to `ws://<device>/ws`, or `ws://<relay>/devices/<deviceId>/ws` through the relay. **No login step** — auth is three ordinary commands (`auth hello|login|resume`) and is off entirely while `web.password` is empty, which is the default.
3. Send a chunk: `[sid u16 LE][flags u8][payload]`, payload starting with the envelope line `{"type":"<category> <command>", …args}\n`, body after it — same chunk or further chunks sharing the sid. `FLAG_FINAL` (0x01) on the last.
4. Read chunks with your sid until one carries `FLAG_FINAL` (or `FLAG_REJECT` 0x02, payload = reason). **Skip session 0** — those are log broadcasts, not your reply. Non-final chunks are reply data, or progress on a long write.
5. Keep each payload inside the transport's inbound window (4096 on both) — a larger frame is refused, not split.

`help list` enumerates every category and command off the device, and `help list -category X -command Y` returns that command's declared arguments, so a probe script needs no source to know what to send.

### Where docs go — three places, nothing else

- **[docs/next-up.md](docs/next-up.md) — what is being worked on *right now*.** Read it first. It is rewritten constantly and deliberately kept tiny: an item is **removed** the moment it lands or is dropped, never annotated, never ticked off in place. Only active work belongs here. If something wants to persist, it does not go in this file — it goes to the backlog (if it is work) or to a note (if it is understanding).
- **`docs/backlog/` — work for later.** One file per topic. A resolved item is **deleted**, not left with a DONE banner; what mattered about it lives in a note by then.
- **`docs/reasoning/` — why things are the way they are.** Append-only, immutable once written, one understanding-delta per note, dated. Never edited: a new understanding is a new note, related to the old one via `builds-on` or `supersedes`. This is the durable record — prefer it over prose documentation anywhere.

Design documents, implementation plans and an ideas folder were all removed on 2026-08-05: they asserted the present tense, so they rotted faster than they were read (see `docs/reasoning/2026-08-05-15h29-a-document-asserts-the-present-tense-so-it-rots.md`). A plan goes in the backlog; the reasoning behind it goes in a note; how to operate something goes in this file. Deleting a doc is not losing it — git has it.

## Architecture

### Three layers, each with a context and a provider

Dependencies run one way only, bottom to top:

Every layer is the same pair: a **context** owning the layer's instances, and a **provider** saying what the layer above may reach for. A manager takes exactly one reference — its own layer's provider — and finds everything through it.

| Layer | Context (owns) | Provider (exposes) |
|---|---|---|
| `main/hardware/` — the **board** | `BoardContext` — driver instances, bus hosts | `BoardProvider` ([hardware/interfaces/BoardProvider.h](main/hardware/interfaces/BoardProvider.h)) — the roles a board owes |
| `main/strux/` — the **framework** | `StruxContext` — the ten Strux managers | `StruxProvider` ([main/strux/StruxProvider.h](main/strux/StruxProvider.h)) |
| `main/app/` — the **application** | `AppContext` — this product's managers | `AppProvider` ([main/app/AppProvider.h](main/app/AppProvider.h)) |

[main.cpp](main/main.cpp) is four calls: `board.Init()`, `strux.Init()`, `application.Init()`, then the OTA validity mark. **The order *within* a layer lives in that layer's context**, not here — `StruxContext::Init()` carries Strux's ordering and its constraints (Relay after WebServer, whose `Authenticator` it shares; Telemetry after Relay, down whose pipe it leaves), so a fork pulling a new framework manager gets its position along with it.

Two rules keep the graph acyclic, and both matter more than they look:

- **Strux never reaches up, and never sideways into hardware.** `StruxProvider` has no `getBoard()` and no way to see `AppProvider`. Hardware belongs to the application: a framework that called `GetLed()` would put that role on `BoardProvider` and oblige every board in every fork to bind one.
- **Anything the framework needs from the application is *registered*, not fetched.** The app registers commands, settings and telemetry points into Strux from its own `Init()`. A Strux manager wanting `AppProvider&` is a design error, not a missing accessor.

`BoardProvider` declares **roles only** (`Led&` today), and that boundary is what keeps it from becoming the union of every board's peripherals: concrete driver accessors — the escape hatch for when the application needs a driver's full API — stay on `BoardContext` itself, checked at compile time. So the day one board grows a display, no other board owes a `MockDisplay`. `AppProvider::getBoard()` returns `BoardContext&`, not `BoardProvider&`, precisely so that escape hatch stays reachable.

What the provider buys over the older duck-typed `Board` is where the failure lands: a board that forgets a role now fails *in the board*, leaving a pure virtual unimplemented, instead of failing later at a call site in application code. What it costs is that a board must bind every role even if this product never uses it — which was already the discipline (`MockLed` exists for exactly that), so the trade is cheap.

Every manager, in either managed layer:

- takes its layer's provider (`StruxProvider&` or `AppProvider&`) in its constructor and reaches everything through it, never directly,
- has copy/move deleted,
- initializes in `Init()` guarded by an `InitState` (`lib/rtos/InitState.h`), not in the constructor.

Adding a **framework** manager: create the class, add it to `StruxProvider`, `StruxContext` (member *and* the ordered `Init()`), and `STRUX_SOURCES` + `INCLUDE_DIRS_LIST` in [main/CMakeLists.txt](main/CMakeLists.txt). Adding an **application** manager: the same, against `AppProvider`, `AppContext` and `APP_SOURCES` — and nothing in `strux/` is touched. [main/app/PsuManager/](main/app/PsuManager/) is this product's only application manager and the worked example: it registers two settings, two commands and a telemetry point without a single edit to the framework. (It replaced Strux's `LedManager`, which was the template's throwaway example.)

Note: the two source lists are separated so a fork does not fight the template over one file, but they are still *one* file and still one ESP-IDF component. Making `strux/` a real component is the step that would make syncing a pull rather than a merge; it has not been taken.

### Layer separation within the board

- `main/hardware/` — changes when you swap the board. Depends on nothing above it: `BoardContext` takes no provider (drivers take their pins and buses as constructor arguments, so nothing here needs one to find a peer) and no layer above is visible from it. Split into:
  - `boards/<name>/` — one folder per target board: `BoardConfig.h` (pins/constants), `BoardContext.h`/`BoardContext.cpp` (the board's `BoardContext : BoardProvider` — owns every driver instance and bus host; `BoardContext.cpp` is added via `BOARD_SOURCES` in the `board.cmake` fragment), and an optional `sdkconfig.defaults` overlaying the common root one. Selected with `-DBOARD=<name>`; only the chosen board folder is on the include path, so `#include "BoardConfig.h"` and `#include "BoardContext.h"` resolve to it.
  - `interfaces/` — the role interfaces in application vocabulary (`Led`), 1–3 pure-virtual methods each, never chip or GPIO vocabulary — plus `BoardProvider`, which assembles them into the list every board owes. Drivers implement the roles (`GpioLed : Led`); a board without the hardware binds a mock (`MockLed`). Adding a role to `BoardProvider` obliges every board to bind it, so add one only when application code speaks in that role; when the application needs a driver's full API, expose a concrete accessor from `BoardContext` instead and leave `BoardProvider` alone (escape hatch). Multi-instance roles get a semantic enum (`Sensor::Ambient`, never `Sensor_2`) mapped by the board — introduce it with the first multi-instance role.
  - `drivers/` — board-independent chip/peripheral drivers shared by boards (e.g. `GpioLed.h`), taking pins/buses as constructor parameters (passed by the board from its `BoardConfig` constants).
- `main/strux/` — the framework: the ten managers. Changes when the template improves.
- `main/app/` — changes when you add a feature to *this* product. Hardware driver *instances* live in the board's `BoardContext` class, reached via `AppProvider::getBoard()`.
- `main/lib/` — the substrate all three layers stand on, and **not** part of any of them: RTOS wrappers (`Task`, `Mutex`, `Timer`), `Stream`/`MemoryStream`/`BufferStream`, `JsonWriter`/`JsonReader`, `DateTime`/`TimeSpan`. Rarely changes. The request/reply seam (`CommandContext`, `ArgReader`, `ReplyWriter`) lives in `lib/protocol/`. It sits beside the layers rather than inside `strux/` because the board layer uses `InitState` and the application uses `Timer` — under `strux/` both would be reaching into the framework for them, which is exactly what the layering forbids. The test for whether something belongs here: it names no layer.

Every folder is on the include path, so headers are included by name alone (`#include "Stream.h"`) and moving one between layers does not touch its callers.

Note: `board.cmake` fragments cannot change component `REQUIRES` (ESP-IDF resolves those in an early pass without the `BOARD` cache var). IDF built-in deps go in `COMPONENT_REQUIRES` in [main/CMakeLists.txt](main/CMakeLists.txt); managed components go in [main/idf_component.yml](main/idf_component.yml).

### Commands (the device's RPC surface)

`CommandManager` is a pure dispatcher — it knows no commands and no other managers. Each command lives in the manager that owns its domain:

- Handlers have the signature `void Handler(Stream& in, Stream& out)`: `in` carries the request payload, the handler writes its complete reply to `out`. Streams are the contract; JSON is a dialect the handler opts into by constructing `JsonReader`/`JsonObject` on line one. Binary payloads (e.g. firmware chunks) use the same contract.
- Owners declare an `inline static CommandEntry commands_[]` table ([CommandEntry.h](main/strux/CommandManager/CommandEntry.h)) with `InvokeCommand<&Owner::Method>` trampolines, and hand it to `CommandManager::Register()` from their `Init()`. Tables must have static storage duration — a registered entry that dies aborts with `FATAL`.
- `help list` is the registry describing itself and the one command `CommandManager` owns: categories and names come off the chain, and a command's *arguments* come from the command itself, by re-dispatching it with a `DescribeArgReader` that prints the declarations instead of filling them and stops the handler at its own `RETURN_IF_ERROR`. So calling `ctx.readArgs(...)` is not optional — a handler that skips it has no `help` and, worse, runs its body when described (logged as an error).
- Two transports reach `Execute()`, and they differ *only* below `SessionLink` ([SessionLink.h](main/lib/protocol/SessionLink.h) — the protocol layer lives in `lib/protocol/`, not under a transport, because the transports depend on it and not the reverse): the local browser WebSocket (`WsSessionLink`, frames read on the httpd task) and the outbound relay pipe (`RelaySessionLink`, frames read on the relay's own task via [RelaySocket](main/strux/RelayManager/RelaySocket.h), a WebSocket driven at the transport layer rather than through `esp_websocket_client` — a callback-delivered frame cannot be the bottom of a streaming handler, and going one layer down is what removed the queue, the per-frame `malloc` and the dropped chunks). Both transports therefore *read* on the task that runs the command. Above that seam everything is shared — `Session` (the stream), `protocol::RunCommandSession` in [CommandEnvelope.h](main/lib/protocol/CommandEnvelope.h) (names the request, dispatches it, closes or refuses the reply), and `AuthGate` — so no handler knows or cares which transport it is serving. There is no HTTP command route; HTTP serves static files only. Wire format is binary session chunks `[session:u16 LE][flags:u8][payload]`, not a JSON envelope.
- Remote access works: `RelayManager` dials out to a server so the device is reachable off-LAN, and the server pulls the device's own frontend with the ordinary `getWebFile` command. The server is **not in this repo** — one relay serves many devices, so it lives in [Strux](https://github.com/vanBassum/Strux/tree/main/relay-server) and its GHCR image is published from there. What is proven and what is left: [docs/backlog/2026-07-03-remote-access.md](docs/backlog/2026-07-03-remote-access.md). Off by default (`relay.enabled`).

Log lines broadcast to all WebSocket clients via `ConsoleManager`. The frontend side is a singleton `BackendService` ([frontend/src/lib/backend.ts](frontend/src/lib/backend.ts)) that matches replies to requests by id and auto-reconnects.

`UpdateManager`'s entire external surface is its command table: session-based updates addressed by partition label (`updateBegin`/`updateWrite`/`updateEnd`), pull OTA from URL, and partition download. App partitions go through `esp_ota_*` (image validation, running slot refused); data partitions are raw erase+write. The built frontend is gzipped into `www/` and flashed as a FAT partition, updatable independently of the app.

### Settings

Settings are typed leaf objects (`lib`-style, [TypedSettings.h](main/strux/SettingsManager/TypedSettings.h)) declared in the manager that owns them and registered at runtime:

```cpp
inline static UInt32Setting port_{ "myfeature.port", "My Feature Port", 1883 };
// in Init():  settings.Register({ &port_ });
uint32_t p = port_.Get();   // NVS value or the typed default
```

`SettingsManager` is the NVS link; the settings UI is generated dynamically from the registered definitions.

**A key is at most 15 characters** — NVS's limit, asserted in `Register()` at *runtime*, so an over-long key compiles fine and then boot-loops the device on the assert. Nothing catches it earlier. `telemetry.enabled` (17) does not fit; `telem.enabled` does.

### Deliberately out of scope

**MQTT and Home Assistant.** This firmware had both before the Strux port and they were deliberately not carried across. Strux removed them upstream on 2026-07-06 on the grounds that a device existing to live in Home Assistant is better served by ESPHome, and this product wants its own UI plus relay-based remote access instead. The old `MqttManager` and `HomeAssistantManager` are in this repo's history (branch `main` before the port, and Strux's own history at `4a41d74`) if that judgement ever needs revisiting — do not write new ones.

**The relay server.** Device side only. See the Commands section.

## Pulling from Strux

The layer split is what makes this cheap, so keep it intact: `strux/` should stay byte-identical to upstream, and everything this product adds should live in `app/`, `hardware/`, or the frontend. Pulling an improvement is then copying directories rather than merging:

```bash
cp -r ../Strux/main/strux    main/
cp -r ../Strux/main/lib      main/          # but see the ArgType::Float note
cp -r ../Strux/docs/reasoning docs/
```

`main/CMakeLists.txt` splits `STRUX_SOURCES` from `APP_SOURCES` for exactly this reason — a new framework manager means adding a line to the first list, and nothing else here moves. Note the two lists are still *one* file and one ESP-IDF component; making `strux/` a real component is the step that would turn this into a proper dependency, and it has not been taken upstream either.

**One fork edit lives outside those directories, and it is debt:** `lib/protocol/` gained `ArgType::Float` (enum value, two `Required`/`Optional` overloads, one `case` each in `JsonArgReader` and `DescribeArgReader`). The reply side already had `value(float)`, so the asymmetry is the argument that it belongs upstream. Until it is contributed back, a naive copy of `lib/protocol/` from Strux will silently break `psu set`. See `docs/reasoning/2026-08-06-20h47`.

## Conventions

- C++17, no exceptions/RTTI-heavy patterns; `snprintf` with `sizeof` bounds, no `strcpy`/`strcat`.
- A command's reply is written through `ctx.reply`, never by naming a format: `ReplyWriter` ([main/lib/protocol/ReplyWriter.h](main/lib/protocol/ReplyWriter.h)) is the mirror of `ArgReader`, and `JsonReplyWriter` is the only implementation today. Every scope comes from a factory — the root from `ctx.reply.object()`/`.array()`, children from their parent — so a call site never spells a type (`auto` is enough) and the methods on offer are exactly the enclosing scope's. Scopes are RAII and close on every return path; writing to a closed one is `FATAL`. It is not a builder: bytes go to the transport on every field, so a scope must close before anything else touches `ctx.out` (see `getWebFile`'s header line, and `updateWrite`'s progress records).
- Elsewhere, JSON is generated with `lib/json/JsonWriter.h` and parsed with `JsonReader` (no external JSON lib). `JsonWriter` is now only the log broadcast, which is not a reply.
- Firmware version derives from the latest git tag (`v0.1.0` → `0.1.0`) in the root CMakeLists.
