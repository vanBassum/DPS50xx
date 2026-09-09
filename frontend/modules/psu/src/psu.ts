// The module's own view of its own commands.
//
// This used to be split between the shell's `backend.ts` (the types) and a
// `use-psu` hook beside it (the polling). Neither was the right home: the shape of
// `psu get`'s reply belongs to whoever owns the `psu` commands, and that is
// PsuManager on the device and this module in the browser. The shell now knows
// nothing about power supplies.

import { useCallback, useEffect, useRef, useState } from "react"
import type { DeviceTransport } from "@shell/contract"

/** One capability of the supply: something it can do or report, already
 *  normalized by its driver.
 *
 *  This is NOT a device register. An XY6020L keeps its runtime in three registers
 *  and reports an unconnected probe as 8888; neither of those facts reaches this
 *  file, because the driver turns storage into meaning before anything gets here.
 *  So there is no protection-label table, no "888.8 means no sensor", and no
 *  h/m/s arithmetic in React — only `kind`, `group`, `unit` and a value.
 *
 *  A capability this build has never heard of still renders: that is the point of
 *  the schema travelling with the value. */
export interface PsuCapability {
  label: string
  unit: string
  /** How to read `value`: a measurement, a 0/1, a code with `options`, or a whole
   *  number of SECONDS this side formats as HH:MM:SS. */
  kind: "number" | "bool" | "enum" | "duration"
  /** Which section of the UI it belongs to — the DRIVER decides, so this file
   *  keeps no list of keys to sort them by. */
  group: "core" | "session" | "protection" | "info"
  value: number
  /** Absent means present. False is the supply saying it has no value right now —
   *  an unplugged probe, or registers that did not answer. */
  available?: boolean
  /** Enums only: the supply's own name for the code in `value`. */
  valueLabel?: string
  /** Enums only: every code's name, in order, so a selector can be drawn. */
  options?: string[]
  access: "r" | "rw"
  /** Writable capabilities only, in the same units as `value`. The supply's own
   *  limits — the browser hardcodes no 50 V and no 20 A. */
  min?: number
  max?: number
}

export interface PsuData {
  /** False when the supply missed enough consecutive polls to be declared gone —
   *  the values are then the last ones successfully read. */
  online: boolean
  /** Keyed by capability name, in the driver's declaration order. */
  capabilities: Record<string, PsuCapability>
}

/** The capabilities this page addresses BY MEANING: it reads three out, edits two,
 *  toggles two, and names three in a status line. Everything else it renders from
 *  `group` without knowing what it is. These are semantic names — no supply's
 *  register map appears here. */
export const PSU_KEYS = {
  setVoltage: "setVoltage",
  setCurrent: "setCurrent",
  outputVoltage: "outputVoltage",
  outputCurrent: "outputCurrent",
  outputPower: "outputPower",
  inputVoltage: "inputVoltage",
  outputEnabled: "outputEnabled",
  keyLock: "keyLock",
  constantCurrent: "constantCurrent",
  protectionState: "protectionState",
  activePreset: "activePreset",
} as const

/** Every field optional — the device leaves an omitted one alone. */
export interface PsuSetpoints {
  voltage?: number
  current?: number
  output?: boolean
  keyLock?: boolean
  backlight?: number
}

/** What the supply holds after the write, echoed under CAPABILITY keys so it folds
 *  straight back into what `psu get` gave. Every field optional because the device
 *  echoes only what it actually has — a supply with no display to light sends no
 *  `backlight`. */
export interface PsuSetResult {
  ok: boolean
  /** Present only when ok is false: the Modbus error, or a rejected setpoint. */
  error?: string
  setVoltage?: number
  setCurrent?: number
  outputEnabled?: boolean
  keyLock?: boolean
  backlight?: number
}

export interface PsuWriteResult {
  ok: boolean
  error?: string
  key?: string
  value?: number
}

export function psuCapability(d: PsuData | null, key: string): PsuCapability | undefined {
  return d?.capabilities?.[key]
}

export function psuNumber(d: PsuData | null, key: string, fallback = 0): number {
  return psuCapability(d, key)?.value ?? fallback
}

export function psuFlag(d: PsuData | null, key: string): boolean {
  return (psuCapability(d, key)?.value ?? 0) !== 0
}

/** Every capability in one group, in the order the driver declared them. This is
 *  what replaced a hardcoded list of "extra" keys. */
export function psuGroup(
  d: PsuData | null,
  group: PsuCapability["group"],
): [string, PsuCapability][] {
  return Object.entries(d?.capabilities ?? {}).filter(([, c]) => c.group === group)
}

/** True when the supply has a value for this right now. */
export function psuAvailable(c: PsuCapability | undefined): boolean {
  return c !== undefined && c.available !== false
}

/** The keys `psu set` echoes back. Named here rather than derived, because the echo
 *  is a promise about specific setpoints and not a schema. */
const ECHOED = ["setVoltage", "setCurrent", "outputEnabled", "keyLock", "backlight"] as const

/** Fold the reply's echo into the capabilities, so a button feels immediate instead
 *  of waiting for the next poll. A key the supply does not have is simply absent
 *  from both sides. */
function applyEcho(prev: PsuData, res: PsuSetResult): PsuData {
  const capabilities = { ...prev.capabilities }
  for (const key of ECHOED) {
    const echoed = res[key]
    const capability = capabilities[key]
    if (echoed === undefined || capability === undefined) continue
    capabilities[key] = {
      ...capability,
      value: typeof echoed === "boolean" ? (echoed ? 1 : 0) : echoed,
      available: true,
    }
  }
  return { ...prev, capabilities }
}

export function errorMessage(e: unknown): string {
  return e instanceof Error ? e.message : String(e)
}

/** Poll `psu get`, and offer the writes. One second on the LAN is what the device's
 *  own poll interval is; through the relay every call takes that device's request
 *  gate, so this is deliberately one command that answers in one round trip rather
 *  than a call per value. */
export function usePsu(transport: DeviceTransport, pollIntervalMs = 1000) {
  const [data, setData] = useState<PsuData | null>(null)
  const [error, setError] = useState<string | null>(null)

  // A reply can land after unmount — the contract has no cancellation — so every
  // setState is guarded rather than assumed safe.
  const alive = useRef(true)
  useEffect(() => {
    alive.current = true
    return () => {
      alive.current = false
    }
  }, [])

  // The poll must not queue a second request behind a slow one: the device is
  // single-in-flight, and a `psu set` waiting on Modbus can hold the wire for its
  // whole timeout. Skipping a tick is better than building a backlog.
  const inFlight = useRef(false)

  const refresh = useCallback(async () => {
    if (inFlight.current) return
    inFlight.current = true
    try {
      const next = await transport.request<PsuData>("psu get")
      if (alive.current) setData(next)
    } catch {
      /* a dropped poll is not worth surfacing — the shell's own status says */
    } finally {
      inFlight.current = false
    }
  }, [transport])

  useEffect(() => {
    refresh()
    const timer = setInterval(refresh, pollIntervalMs)
    return () => clearInterval(timer)
  }, [refresh, pollIntervalMs])

  /** Apply any subset of the setpoints in ONE command — the device leaves an omitted
   *  field alone and skips the Modbus write for anything unchanged. The reply already
   *  carries the resulting state, so it lands in `data` without waiting for the next
   *  poll, which is what makes a button feel immediate. */
  const apply = useCallback(
    async (changes: PsuSetpoints) => {
      try {
        const res = await transport.request<PsuSetResult>("psu set", { ...changes })
        if (!alive.current) return
        setError(res.ok ? null : (res.error ?? "command failed"))
        setData((prev) => (prev ? applyEcho(prev, res) : prev))
      } catch (e) {
        if (alive.current) setError(errorMessage(e))
      }
    },
    [transport],
  )

  const setVoltage = useCallback((v: number) => apply({ voltage: v }), [apply])
  const setCurrent = useCallback((a: number) => apply({ current: a }), [apply])
  const setOutput = useCallback((on: boolean) => apply({ output: on }), [apply])
  const setKeyLock = useCallback((locked: boolean) => apply({ keyLock: locked }), [apply])

  /** Write ONE capability by name, validated on the device against that
   *  capability's own range. This is how the long tail — protection thresholds and
   *  whatever the next supply adds — is configured without this file growing a
   *  method per register. */
  const write = useCallback(
    async (key: string, value: number) => {
      try {
        const res = await transport.request<PsuWriteResult>("psu write", { key, value })
        if (alive.current) setError(res.ok ? null : (res.error ?? "command failed"))
        if (res.ok) await refresh()
        return res.ok
      } catch (e) {
        if (alive.current) setError(errorMessage(e))
        return false
      }
    },
    [transport, refresh],
  )

  return { data, error, apply, write, setVoltage, setCurrent, setOutput, setKeyLock, refresh }
}

/** The only presentation knowledge this module needs, and all of it comes from
 *  `kind`: seconds become HH:MM:SS, a code becomes its name, and a capability the
 *  supply has no value for becomes an em dash rather than a number. */
export function format(c: PsuCapability): string {
  if (!psuAvailable(c)) return "—"
  if (c.kind === "duration") return formatDuration(c.value)
  if (c.kind === "bool") return c.value !== 0 ? "On" : "Off"
  if (c.kind === "enum") return c.valueLabel ?? String(c.value)
  return `${c.value.toFixed(2)}${c.unit ? ` ${c.unit}` : ""}`
}

export function formatDuration(totalSeconds: number): string {
  const s = Math.max(0, Math.floor(totalSeconds))
  const pad = (n: number) => String(n).padStart(2, "0")
  return `${pad(Math.floor(s / 3600))}:${pad(Math.floor((s % 3600) / 60))}:${pad(s % 60)}`
}
