import { useState } from "react"
import { useDPS5020, PROTECTION_LABELS, type HistoryPoint } from "@/hooks/use-dps5020"
import { useDeviceInfo } from "@/hooks/use-device-info"
import { Button } from "@/components/ui/button"
import { Input } from "@/components/ui/input"
import { PowerIcon, ZapIcon, LockIcon, UnlockIcon, CpuIcon } from "lucide-react"
import {
  LineChart,
  Line,
  XAxis,
  YAxis,
  CartesianGrid,
  Tooltip,
  ReferenceLine,
  ResponsiveContainer,
  Legend,
} from "recharts"

function formatBytes(bytes: number): string {
  if (bytes < 1024) return `${bytes} B`
  return `${(bytes / 1024).toFixed(1)} KB`
}

export default function HomePage() {
  const { data, history, setVoltage, setCurrent, setOutput, setKeyLock } = useDPS5020()
  const info = useDeviceInfo()

  return (
    <div className="space-y-6">
      <div className="flex items-center justify-between">
        <div className="flex items-center gap-2">
          <ZapIcon className="size-5 text-muted-foreground" />
          <h1 className="text-2xl font-bold">DPS5020</h1>
        </div>
        {data && (
          <span
            className={`rounded-full px-2.5 py-0.5 text-xs font-medium ${
              data.online
                ? "bg-emerald-500/15 text-emerald-500"
                : "bg-red-500/15 text-red-500"
            }`}
          >
            {data.online ? "Online" : "Offline"}
          </span>
        )}
      </div>

      {!data && (
        <div className="rounded-xl border bg-card p-8 text-center text-sm text-muted-foreground">
          Connecting to device...
        </div>
      )}

      {data && !data.online && (
        <div className="rounded-xl border border-amber-500/20 bg-card p-8 text-center text-sm text-amber-500">
          DPS5020 not responding on Modbus. Check wiring and power.
        </div>
      )}

      {data?.online && (
        <div className="grid gap-6 lg:grid-cols-[1fr_1fr]">
          {/* Left column: readouts, status, controls, device info */}
          <div className="space-y-4">
            {/* Live readouts */}
            <div className="grid grid-cols-3 gap-3">
              <ReadoutCard label="Voltage" value={data.outVoltage} unit="V" color="text-yellow-400" />
              <ReadoutCard label="Current" value={data.outCurrent} unit="A" color="text-cyan-400" />
              <ReadoutCard label="Power" value={data.outPower} unit="W" color="text-orange-400" />
            </div>

            {/* Status bar */}
            <div className="flex items-center justify-between rounded-lg border bg-card px-4 py-2.5 text-sm">
              <StatusItem label="Input" value={`${data.inVoltage.toFixed(1)}V`} />
              <Divider />
              <StatusItem
                label="Mode"
                value={data.constantCurrent ? "CC" : "CV"}
                highlight={data.constantCurrent}
              />
              <Divider />
              <StatusItem
                label="Protection"
                value={PROTECTION_LABELS[data.protection] ?? "?"}
                highlight={data.protection !== 0}
              />
            </div>

            {/* Controls */}
            <div className="rounded-xl border bg-card p-6 text-card-foreground shadow-sm space-y-5">
              <SetpointRow
                label="Set Voltage"
                unit="V"
                current={data.setVoltage}
                min={0}
                max={50}
                step={0.01}
                onSet={setVoltage}
              />
              <SetpointRow
                label="Set Current"
                unit="A"
                current={data.setCurrent}
                min={0}
                max={20}
                step={0.01}
                onSet={setCurrent}
              />

              <div className="flex items-center gap-3 pt-2">
                <Button
                  className={`flex-1 h-12 text-base font-bold ${
                    data.outputOn
                      ? "bg-emerald-600 hover:bg-emerald-700 text-white"
                      : "bg-red-600 hover:bg-red-700 text-white"
                  }`}
                  onClick={() => setOutput(!data.outputOn)}
                >
                  <PowerIcon className="mr-2 size-5" />
                  {data.outputOn ? "OUTPUT ON" : "OUTPUT OFF"}
                </Button>

                <Button
                  variant="outline"
                  size="icon"
                  className="h-12 w-12 shrink-0"
                  onClick={() => setKeyLock(!data.keyLock)}
                  title={data.keyLock ? "Unlock keys" : "Lock keys"}
                >
                  {data.keyLock ? (
                    <LockIcon className="size-5 text-amber-500" />
                  ) : (
                    <UnlockIcon className="size-5" />
                  )}
                </Button>
              </div>
            </div>

            {/* Device info */}
            {info && (
              <div className="rounded-xl border bg-card p-6 text-card-foreground shadow-sm">
                <div className="mb-3 flex items-center gap-2">
                  <CpuIcon className="size-4 text-muted-foreground" />
                  <h2 className="text-sm font-semibold">Device</h2>
                </div>
                <div className="grid grid-cols-2 gap-x-8 gap-y-1.5 text-xs">
                  <InfoRow label="Firmware" value={info.firmware} />
                  <InfoRow label="ESP-IDF" value={info.idf} />
                  <InfoRow label="Chip" value={info.chip} />
                  <InfoRow label="Free heap" value={formatBytes(info.heapFree)} />
                </div>
              </div>
            )}
          </div>

          {/* Right column: charts */}
          {history.length > 1 && (
            <div className="space-y-4">
              <VoltageCurrentChart history={history} setVoltage={data.setVoltage} setCurrent={data.setCurrent} />
              <PowerChart history={history} />
            </div>
          )}
        </div>
      )}
    </div>
  )
}

// ── Charts ───────────────────────────────────────────────────

const chartStyle = {
  grid: "#333",
  tooltip: { backgroundColor: "#1a1a1a", border: "1px solid #333", borderRadius: 6 },
  tick: { fill: "#888", fontSize: 11 },
}

function VoltageCurrentChart({
  history,
  setVoltage,
  setCurrent,
}: {
  history: HistoryPoint[]
  setVoltage: number
  setCurrent: number
}) {
  return (
    <div className="rounded-xl border bg-card p-4 shadow-sm">
      <div className="mb-2 text-xs font-medium text-muted-foreground">Voltage & Current</div>
      <ResponsiveContainer width="100%" height={220}>
        <LineChart data={history}>
          <CartesianGrid strokeDasharray="3 3" stroke={chartStyle.grid} />
          <XAxis
            dataKey="time"
            tick={chartStyle.tick}
            interval="preserveStartEnd"
            minTickGap={60}
          />
          <YAxis
            yAxisId="v"
            tick={chartStyle.tick}
            width={40}
            domain={[0, "auto"]}
            label={{ value: "V", position: "insideTopLeft", fill: "#facc15", fontSize: 11, dy: -10 }}
          />
          <YAxis
            yAxisId="a"
            orientation="right"
            tick={chartStyle.tick}
            width={40}
            domain={[0, "auto"]}
            label={{ value: "A", position: "insideTopRight", fill: "#22d3ee", fontSize: 11, dy: -10 }}
          />
          <Tooltip contentStyle={chartStyle.tooltip} labelStyle={{ color: "#aaa" }} />
          <Legend />
          <ReferenceLine
            yAxisId="v"
            y={setVoltage}
            stroke="#facc15"
            strokeDasharray="6 3"
            strokeOpacity={0.5}
            label={{ value: `${setVoltage.toFixed(1)}V`, fill: "#facc15", fontSize: 10, position: "left" }}
          />
          <ReferenceLine
            yAxisId="a"
            y={setCurrent}
            stroke="#22d3ee"
            strokeDasharray="6 3"
            strokeOpacity={0.5}
            label={{ value: `${setCurrent.toFixed(1)}A`, fill: "#22d3ee", fontSize: 10, position: "right" }}
          />
          <Line
            yAxisId="v"
            type="monotone"
            dataKey="voltage"
            name="Voltage"
            stroke="#facc15"
            strokeWidth={2}
            dot={false}
            isAnimationActive={false}
          />
          <Line
            yAxisId="a"
            type="monotone"
            dataKey="current"
            name="Current"
            stroke="#22d3ee"
            strokeWidth={2}
            dot={false}
            isAnimationActive={false}
          />
        </LineChart>
      </ResponsiveContainer>
    </div>
  )
}

function PowerChart({ history }: { history: HistoryPoint[] }) {
  return (
    <div className="rounded-xl border bg-card p-4 shadow-sm">
      <div className="mb-2 text-xs font-medium text-muted-foreground">Power</div>
      <ResponsiveContainer width="100%" height={160}>
        <LineChart data={history}>
          <CartesianGrid strokeDasharray="3 3" stroke={chartStyle.grid} />
          <XAxis
            dataKey="time"
            tick={chartStyle.tick}
            interval="preserveStartEnd"
            minTickGap={60}
          />
          <YAxis
            tick={chartStyle.tick}
            width={45}
            domain={[0, "auto"]}
            label={{ value: "W", position: "insideTopLeft", fill: "#fb923c", fontSize: 11, dy: -10 }}
          />
          <Tooltip contentStyle={chartStyle.tooltip} labelStyle={{ color: "#aaa" }} />
          <Line
            type="monotone"
            dataKey="power"
            name="Power"
            stroke="#fb923c"
            strokeWidth={2}
            dot={false}
            isAnimationActive={false}
          />
        </LineChart>
      </ResponsiveContainer>
    </div>
  )
}

// ── Subcomponents ────────────────────────────────────────────

function ReadoutCard({
  label,
  value,
  unit,
  color,
}: {
  label: string
  value: number | undefined
  unit: string
  color: string
}) {
  return (
    <div className="rounded-xl border bg-card p-4 text-center shadow-sm">
      <div className={`text-3xl font-bold tabular-nums ${color}`}>
        {value !== undefined ? value.toFixed(2) : "--.-"}
      </div>
      <div className="mt-1 text-xs text-muted-foreground">
        {label} ({unit})
      </div>
    </div>
  )
}

function SetpointRow({
  label,
  unit,
  current,
  min,
  max,
  step,
  onSet,
}: {
  label: string
  unit: string
  current: number
  min: number
  max: number
  step: number
  onSet: (v: number) => void
}) {
  const [value, setValue] = useState("")
  const [editing, setEditing] = useState(false)

  function handleSubmit() {
    const num = parseFloat(value)
    if (!isNaN(num) && num >= min && num <= max) {
      onSet(num)
    }
    setEditing(false)
    setValue("")
  }

  return (
    <div className="flex items-center justify-between gap-4">
      <div>
        <div className="text-sm font-medium">{label}</div>
        <div className="font-mono text-xs text-muted-foreground">
          {current.toFixed(2)} {unit}
        </div>
      </div>
      <div className="flex items-center gap-2">
        <Input
          className="w-28 text-right font-mono"
          type="number"
          min={min}
          max={max}
          step={step}
          placeholder={current.toFixed(2)}
          value={value}
          onChange={(e) => {
            setValue(e.target.value)
            setEditing(true)
          }}
          onKeyDown={(e) => {
            if (e.key === "Enter") handleSubmit()
          }}
        />
        <span className="w-4 text-sm text-muted-foreground">{unit}</span>
        {editing && (
          <Button size="sm" onClick={handleSubmit}>
            Set
          </Button>
        )}
      </div>
    </div>
  )
}

function StatusItem({
  label,
  value,
  highlight,
}: {
  label: string
  value: string
  highlight?: boolean
}) {
  return (
    <div className="text-center">
      <div className="text-xs text-muted-foreground">{label}</div>
      <div
        className={`font-mono text-sm font-medium ${highlight ? "text-amber-500" : ""}`}
      >
        {value}
      </div>
    </div>
  )
}

function Divider() {
  return <div className="h-8 w-px bg-border" />
}

function InfoRow({ label, value }: { label: string; value: string }) {
  return (
    <div className="flex justify-between">
      <span className="text-muted-foreground">{label}</span>
      <span className="font-mono">{value}</span>
    </div>
  )
}
