import { useEffect, useState, useCallback, useRef } from "react"
import { backend, type PsuData, type PsuSetpoints } from "@/lib/backend"
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
              voltage: d.outVoltage,
              current: d.outCurrent,
              power: d.outPower,
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
      setData((prev) =>
        prev
          ? {
              ...prev,
              setVoltage: res.setVoltage,
              setCurrent: res.setCurrent,
              outputOn: res.outputOn,
              keyLock: res.keyLock,
              backlight: res.backlight,
            }
          : prev,
      )
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
