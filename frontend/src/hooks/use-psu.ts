import { useEffect, useState, useCallback, useRef } from "react"
import {
  backend,
  type PsuData,
  type PsuSetResult,
  type PsuSetpoints,
} from "@/lib/backend"
import { useConnectionStatus } from "@/hooks/use-connection-status"

/** The keys `psu set` echoes back. Named here rather than derived, because the
 *  echo is a promise about specific setpoints and not a schema. */
const ECHOED = ["setVoltage", "setCurrent", "outputEnabled", "keyLock", "backlight"] as const

/** Fold the reply's echo into the readings, so a button feels immediate instead
 *  of waiting for the next poll. A key the supply does not have is simply
 *  absent from both sides. */
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

export function usePsu(pollIntervalMs = 1000) {
  const connection = useConnectionStatus()
  const [data, setData] = useState<PsuData | null>(null)
  const [error, setError] = useState<string | null>(null)

  // A reply can land after unmount — a command in flight is not cancellable — so
  // every setState below is guarded rather than assumed safe.
  const alive = useRef(true)
  useEffect(() => {
    alive.current = true
    return () => {
      alive.current = false
    }
  }, [])

  // The poll must not queue a second request behind a slow one: the device is
  // single-in-flight, and a `psu set` waiting on Modbus can hold the wire for
  // its whole timeout. Skipping a tick is better than building a backlog.
  const inFlight = useRef(false)

  const refresh = useCallback(async () => {
    if (inFlight.current) return
    inFlight.current = true
    try {
      const next = await backend.getPsu()
      if (alive.current) setData(next)
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
      if (!alive.current) return
      setError(res.ok ? null : (res.error ?? "command failed"))
      setData((prev) => (prev ? applyEcho(prev, res) : prev))
    } catch (e) {
      if (alive.current) setError(e instanceof Error ? e.message : "command failed")
    }
  }, [])

  const setVoltage = useCallback((v: number) => apply({ voltage: v }), [apply])
  const setCurrent = useCallback((a: number) => apply({ current: a }), [apply])
  const setOutput = useCallback((on: boolean) => apply({ output: on }), [apply])
  const setKeyLock = useCallback((locked: boolean) => apply({ keyLock: locked }), [apply])

  /** Write one capability by name and refresh, so a protections panel needs no
   *  echo protocol of its own. */
  const write = useCallback(
    async (key: string, value: number) => {
      try {
        const res = await backend.writePsu(key, value)
        if (alive.current) setError(res.ok ? null : (res.error ?? "command failed"))
        if (res.ok) await refresh()
        return res.ok
      } catch (e) {
        if (alive.current) setError(e instanceof Error ? e.message : "command failed")
        return false
      }
    },
    [refresh],
  )

  return { data, error, apply, write, setVoltage, setCurrent, setOutput, setKeyLock, refresh }
}
