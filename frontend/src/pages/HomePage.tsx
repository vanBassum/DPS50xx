import { useState } from "react"
import { usePsu, type HistoryPoint } from "@/hooks/use-psu"
import { PROTECTION_LABELS } from "@/lib/backend"
import { Button } from "@/components/ui/button"
import { Input } from "@/components/ui/input"
import { PowerIcon, ZapIcon, LockIcon, UnlockIcon } from "lucide-react"
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

export default function HomePage() {
  const { data, history, error, setVoltage, setCurrent, setOutput, setKeyLock } = usePsu()

  return (
    <div className="space-y-6">
      <div className="flex items-center justify-between">
        <div className="flex items-center gap-2">
          <ZapIcon className="size-5 text-muted-foreground" />
          <h1 className="text-2xl font-bold">Supply</h1>
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
          DPS50xx not responding on Modbus. Check wiring and power.
        </div>
      )}

      {error && (
        <div className="rounded-lg border border-red-500/20 bg-red-500/5 px-4 py-2.5 text-sm text-red-500">
          {error}
        </div>
      )}

      {data?.online && (
        <div className="grid gap-6 lg:grid-cols-[1fr_1fr]">
          {/* Left column: readouts, status, controls */}
          <div className="space-y-4">
            {/* Live readouts */}
            <div className="grid grid-cols-3 gap-3">
              <ReadoutCard label="Voltage" value={data.outVoltage} unit="V" color="text-yellow-500" />
              <ReadoutCard label="Current" value={data.outCurrent} unit="A" color="text-cyan-500" />
              <ReadoutCard label="Power" value={data.outPower} unit="W" color="text-orange-500" />
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
          </div>

          {/* Right column: charts */}
          {history.length > 1 && (
            <div className="space-y-4">
              <VoltageCurrentChart
                history={history}
                setVoltage={data.setVoltage}
                setCurrent={data.setCurrent}
              />
              <PowerChart history={history} />
            </div>
          )}
        </div>
      )}
    </div>
  )
}

// ── Charts ───────────────────────────────────────────────────

// Series colours stay literal — a trace's colour is its identity, and these
// match the readout cards above. Everything structural (grid, ticks, tooltip)
// comes from the theme's CSS variables so the charts follow light/dark with the
// rest of the shell instead of being pinned to the old dark-only palette.
const VOLTAGE_COLOR = "#eab308"
const CURRENT_COLOR = "#06b6d4"
const POWER_COLOR = "#f97316"

const chartStyle = {
  grid: "var(--border)",
  tooltip: {
    backgroundColor: "var(--card)",
    border: "1px solid var(--border)",
    borderRadius: 6,
    color: "var(--card-foreground)",
  },
  tick: { fill: "var(--muted-foreground)", fontSize: 11 },
  label: { color: "var(--muted-foreground)" },
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
      <div className="mb-2 text-xs font-medium text-muted-foreground">Voltage &amp; Current</div>
      <ResponsiveContainer width="100%" height={220}>
        <LineChart data={history}>
          <CartesianGrid strokeDasharray="3 3" stroke={chartStyle.grid} />
          <XAxis dataKey="time" tick={chartStyle.tick} interval="preserveStartEnd" minTickGap={60} />
          <YAxis
            yAxisId="v"
            tick={chartStyle.tick}
            width={40}
            domain={[0, "auto"]}
            label={{ value: "V", position: "insideTopLeft", fill: VOLTAGE_COLOR, fontSize: 11, dy: -10 }}
          />
          <YAxis
            yAxisId="a"
            orientation="right"
            tick={chartStyle.tick}
            width={40}
            domain={[0, "auto"]}
            label={{ value: "A", position: "insideTopRight", fill: CURRENT_COLOR, fontSize: 11, dy: -10 }}
          />
          <Tooltip contentStyle={chartStyle.tooltip} labelStyle={chartStyle.label} />
          <Legend />
          {/* The setpoints as dashed references: how far the output is from what
              was asked for should be readable without comparing two numbers. */}
          <ReferenceLine
            yAxisId="v"
            y={setVoltage}
            stroke={VOLTAGE_COLOR}
            strokeDasharray="6 3"
            strokeOpacity={0.5}
            label={{ value: `${setVoltage.toFixed(1)}V`, fill: VOLTAGE_COLOR, fontSize: 10, position: "left" }}
          />
          <ReferenceLine
            yAxisId="a"
            y={setCurrent}
            stroke={CURRENT_COLOR}
            strokeDasharray="6 3"
            strokeOpacity={0.5}
            label={{ value: `${setCurrent.toFixed(1)}A`, fill: CURRENT_COLOR, fontSize: 10, position: "right" }}
          />
          <Line
            yAxisId="v"
            type="monotone"
            dataKey="voltage"
            name="Voltage"
            stroke={VOLTAGE_COLOR}
            strokeWidth={2}
            dot={false}
            isAnimationActive={false}
          />
          <Line
            yAxisId="a"
            type="monotone"
            dataKey="current"
            name="Current"
            stroke={CURRENT_COLOR}
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
          <XAxis dataKey="time" tick={chartStyle.tick} interval="preserveStartEnd" minTickGap={60} />
          <YAxis
            tick={chartStyle.tick}
            width={45}
            domain={[0, "auto"]}
            label={{ value: "W", position: "insideTopLeft", fill: POWER_COLOR, fontSize: 11, dy: -10 }}
          />
          <Tooltip contentStyle={chartStyle.tooltip} labelStyle={chartStyle.label} />
          <Line
            type="monotone"
            dataKey="power"
            name="Power"
            stroke={POWER_COLOR}
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
      <div className={`font-mono text-sm font-medium ${highlight ? "text-amber-500" : ""}`}>
        {value}
      </div>
    </div>
  )
}

function Divider() {
  return <div className="h-8 w-px bg-border" />
}
