import { useEffect, useState, useCallback, useRef } from "react"
import { backend, type DPS5020Data } from "@/lib/backend"
import { useConnectionStatus } from "@/hooks/use-connection-status"

export interface HistoryPoint {
  time: string
  voltage: number
  current: number
  power: number
}

const MAX_HISTORY = 300 // 5 minutes at 1s poll

export function useDPS5020(pollIntervalMs = 1000) {
  const connection = useConnectionStatus()
  const [data, setData] = useState<DPS5020Data | null>(null)
  const [history, setHistory] = useState<HistoryPoint[]>([])
  const dataRef = useRef(data)
  dataRef.current = data

  const refresh = useCallback(async () => {
    try {
      const d = await backend.getDPS5020()
      setData(d)

      if (d.online) {
        setHistory((prev) => {
          const next = [
            ...prev,
            {
              time: new Date().toLocaleTimeString([], { hour: "2-digit", minute: "2-digit", second: "2-digit" }),
              voltage: d.outVoltage,
              current: d.outCurrent,
              power: d.outPower,
            },
          ]
          return next.length > MAX_HISTORY ? next.slice(-MAX_HISTORY) : next
        })
      }
    } catch {
      // ignore
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

  const setVoltage = useCallback(async (v: number) => {
    await backend.setDPS5020("voltage", v.toString())
    refresh()
  }, [refresh])

  const setCurrent = useCallback(async (a: number) => {
    await backend.setDPS5020("current", a.toString())
    refresh()
  }, [refresh])

  const setOutput = useCallback(async (on: boolean) => {
    await backend.setDPS5020("output", on ? "true" : "false")
    refresh()
  }, [refresh])

  const setKeyLock = useCallback(async (locked: boolean) => {
    await backend.setDPS5020("keyLock", locked ? "true" : "false")
    refresh()
  }, [refresh])

  return { data, history, setVoltage, setCurrent, setOutput, setKeyLock, refresh }
}

export const PROTECTION_LABELS = ["None", "OVP", "OCP", "OPP"] as const
