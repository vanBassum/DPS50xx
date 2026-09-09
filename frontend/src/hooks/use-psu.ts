import { useEffect, useState, useCallback, useRef } from "react"
import {
  backend,
  psuNumber,
  PSU_KEYS,
  type PsuData,
  type PsuSetResult,
  type PsuSetpoints,
} from "@/lib/backend"
import { useConnectionStatus } from "@/hooks/use-connection-status"

export interface HistoryPoint {
  time: string
  voltage: number
  current: number
  power: number
}

/** 5 minutes at the default 1 s poll. History is client-side and deliberately
 *  not persisted — the device's own telemetry (psu.telem) is what survives a
 *  reload, and it goes to InfluxDB through the relay. */
const MAX_HISTORY = 300

/** The keys `psu set` echoes back. Named here rather than derived, because the
 *  echo is a promise about specific setpoints and not a schema. */
const ECHOED = [
  PSU_KEYS.setVoltage,
  PSU_KEYS.setCurrent,
  PSU_KEYS.outputOn,
  PSU_KEYS.keyLock,
  PSU_KEYS.backlight,
] as const

/** Fold the reply's echo into the readings, so a button feels immediate instead
 *  of waiting for the next poll. A key the supply does not have is simply
 *  absent from both sides. */
function applyEcho(prev: PsuData, res: PsuSetResult): PsuData {
  const readings = { ...prev.readings }
  for (const key of ECHOED) {
    const echoed = res[key]
    const reading = readings[key]
    if (echoed === undefined || reading === undefined) continue
    readings[key] = {
      ...reading,
      value: typeof echoed === "boolean" ? (echoed ? 1 : 0) : echoed,
    }
  }
  return { ...prev, readings }
}

export function usePsu(pollIntervalMs = 1000) {
  const connection = useConnectionStatus()
  const [data, setData] = useState<PsuData | null>(null)
  const [history, setHistory] = useState<HistoryPoint[]>([])
  const [error, setError] = useState<string | null>(null)

  // The poll must not queue a second request behind a slow one: the device is
  // single-in-flight, and a `psu set` waiting on Modbus can hold the wire for
  // its whole timeout. Skipping a tick is better than building a backlog.
  const inFlight = useRef(false)

  const refresh = useCallback(async () => {
    if (inFlight.current) return
    inFlight.current = true
    try {
      const d = await backend.getPsu()
      setData(d)

      if (d.online) {
        setHistory((prev) => {
          const next = [
            ...prev,
            {
              time: new Date().toLocaleTimeString([], {
                hour: "2-digit",
                minute: "2-digit",
                second: "2-digit",
              }),
              voltage: psuNumber(d, PSU_KEYS.outVoltage),
              current: psuNumber(d, PSU_KEYS.outCurrent),
              power: psuNumber(d, PSU_KEYS.outPower),
            },
          ]
          return next.length > MAX_HISTORY ? next.slice(-MAX_HISTORY) : next
        })
      }
    } catch {
      /* a dropped poll is not worth surfacing — the connection dot already says */
    } finally {
      inFlight.current = false
    }
  }, [])

  useEffect(() => {
    if (connection !== "connected") {
      setData(null)
      return
    }

    refresh()
    const timer = setInterval(refresh, pollIntervalMs)
    return () => clearInterval(timer)
  }, [connection, refresh, pollIntervalMs])

  /** Apply any subset of the setpoints in one command. The reply already carries
   *  the resulting state, so it lands in `data` without waiting for the next
   *  poll — which is what makes a button feel immediate. */
  const apply = useCallback(async (changes: PsuSetpoints) => {
    try {
      const res = await backend.setPsu(changes)
      setError(res.ok ? null : (res.error ?? "command failed"))
      setData((prev) => (prev ? applyEcho(prev, res) : prev))
    } catch (e) {
      setError(e instanceof Error ? e.message : "command failed")
    }
  }, [])

  const setVoltage = useCallback((v: number) => apply({ voltage: v }), [apply])
  const setCurrent = useCallback((a: number) => apply({ current: a }), [apply])
  const setOutput = useCallback((on: boolean) => apply({ output: on }), [apply])
  const setKeyLock = useCallback((locked: boolean) => apply({ keyLock: locked }), [apply])

  return { data, history, error, apply, setVoltage, setCurrent, setOutput, setKeyLock, refresh }
}
