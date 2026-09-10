import { useState } from "react"
import { usePsu, type HistoryPoint } from "@/hooks/use-psu"
import {
  psuAvailable,
  psuCapability,
  psuFlag,
  psuGroup,
  psuNumber,
  PSU_KEYS,
  type PsuCapability,
  type PsuData,
} from "@/lib/backend"
import { Button } from "@/components/ui/button"
import { Input } from "@/components/ui/input"
import {
  Dialog,
  DialogContent,
  DialogDescription,
  DialogHeader,
  DialogTitle,
  DialogTrigger,
} from "@/components/ui/dialog"
import { PowerIcon, ZapIcon, LockIcon, UnlockIcon, ShieldIcon } from "lucide-react"
import {
  CartesianGrid,
  Legend,
  Line,
  LineChart,
  ReferenceLine,
  ResponsiveContainer,
  Tooltip,
  XAxis,
  YAxis,
} from "recharts"

// This page consumes CAPABILITIES, never registers. It addresses a handful by
// meaning (PSU_KEYS: what it reads out, edits and toggles) and renders the rest
// from the `group` and `kind` the driver sent — so a supply with values this
// build has never heard of still displays, and nothing here knows that an
// XY6020L keeps its runtime in three registers or calls an absent probe 8888.
//
// Two columns, because the numbers and their history are one reading: what the
// supply is doing right now belongs beside where it has been, not a scroll away.
// It collapses to one column below `lg`, where the charts go under the controls.

export default function HomePage() {
  const { data, history, error, write, setVoltage, setCurrent, setOutput, setKeyLock } =
    usePsu()

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
          Supply not responding on Modbus. Check wiring and power.
        </div>
      )}

      {error && (
        <div className="rounded-lg border border-red-500/20 bg-red-500/5 px-4 py-2.5 text-sm text-red-500">
          {error}
        </div>
      )}

      {data?.online && (
        <div className="grid gap-6 lg:grid-cols-[1fr_1fr]">
          {/* Left column: readouts, status, controls, session */}
          <div className="space-y-4">
          {/* Live readouts */}
          <div className="grid grid-cols-3 gap-3">
            <ReadoutCard label="Voltage" value={psuNumber(data, PSU_KEYS.outputVoltage)} unit="V" color="text-yellow-500" />
            <ReadoutCard label="Current" value={psuNumber(data, PSU_KEYS.outputCurrent)} unit="A" color="text-cyan-500" />
            <ReadoutCard label="Power" value={psuNumber(data, PSU_KEYS.outputPower)} unit="W" color="text-orange-500" />
          </div>

          {/* Status bar */}
          <div className="flex items-center justify-between rounded-lg border bg-card px-4 py-2.5 text-sm">
            <StatusItem label="Input" value={`${psuNumber(data, PSU_KEYS.inputVoltage).toFixed(1)}V`} />
            <Divider />
            <StatusItem
              label="Mode"
              value={psuFlag(data, PSU_KEYS.constantCurrent) ? "CC" : "CV"}
              highlight={psuFlag(data, PSU_KEYS.constantCurrent)}
            />
            <Divider />
            {/* The supply names its own protection codes, so there is no label
                table on this side to fall out of date. This is the current TRIP
                STATE — the configured thresholds are behind Protections. */}
            <StatusItem
              label="Protection"
              value={psuCapability(data, PSU_KEYS.protectionState)?.valueLabel ?? "?"}
              highlight={psuNumber(data, PSU_KEYS.protectionState) !== 0}
            />
          </div>

          {/* Controls */}
          <div className="rounded-xl border bg-card p-6 text-card-foreground shadow-sm space-y-5">
            <SetpointRow capability={psuCapability(data, PSU_KEYS.setVoltage)} onSet={setVoltage} />
            <SetpointRow capability={psuCapability(data, PSU_KEYS.setCurrent)} onSet={setCurrent} />

            <div className="flex items-center gap-3 pt-2">
              <Button
                className={`flex-1 h-12 text-base font-bold ${
                  psuFlag(data, PSU_KEYS.outputEnabled)
                    ? "bg-emerald-600 hover:bg-emerald-700 text-white"
                    : "bg-red-600 hover:bg-red-700 text-white"
                }`}
                onClick={() => setOutput(!psuFlag(data, PSU_KEYS.outputEnabled))}
              >
                <PowerIcon className="mr-2 size-5" />
                {psuFlag(data, PSU_KEYS.outputEnabled) ? "OUTPUT ON" : "OUTPUT OFF"}
              </Button>

              {psuCapability(data, PSU_KEYS.keyLock) && (
                <Button
                  variant="outline"
                  size="icon"
                  className="h-12 w-12 shrink-0"
                  onClick={() => setKeyLock(!psuFlag(data, PSU_KEYS.keyLock))}
                  title={psuFlag(data, PSU_KEYS.keyLock) ? "Unlock keys" : "Lock keys"}
                >
                  {psuFlag(data, PSU_KEYS.keyLock) ? (
                    <LockIcon className="size-5 text-amber-500" />
                  ) : (
                    <UnlockIcon className="size-5" />
                  )}
                </Button>
              )}
            </div>

            <div className="flex items-center justify-between gap-3 border-t pt-4">
              <PresetSelect capability={psuCapability(data, PSU_KEYS.activePreset)} />
              <ProtectionsDialog data={data} write={write} />
            </div>
          </div>

          <SessionCard data={data} />
          </div>

          {/* Right column: the same three readings over time. Nothing renders
              until there are two points, because one point is not a trace. */}
          {history.length > 1 && (
            <div className="space-y-4">
              <VoltageCurrentChart
                history={history}
                setVoltage={psuNumber(data, PSU_KEYS.setVoltage)}
                setCurrent={psuNumber(data, PSU_KEYS.setCurrent)}
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

// ── Session ──────────────────────────────────────────────────
//
// Whatever the driver grouped as `session`: charge, energy, runtime, temps. No
// key list here — a supply that accumulates something else shows it without
// this component changing.

function SessionCard({ data }: { data: PsuData }) {
  const entries = psuGroup(data, "session")
  if (entries.length === 0) return null

  return (
    <div className="rounded-xl border bg-card p-4 text-card-foreground shadow-sm">
      <div className="mb-3 text-xs font-medium uppercase tracking-wide text-muted-foreground">
        Session
      </div>
      <div className="grid grid-cols-2 gap-x-6 gap-y-2 sm:grid-cols-3">
        {entries.map(([key, c]) => (
          <div key={key} className="flex items-baseline justify-between gap-2">
            <span className="text-xs text-muted-foreground">{c.label}</span>
            <span className="font-mono text-sm font-medium tabular-nums">{format(c)}</span>
          </div>
        ))}
      </div>
    </div>
  )
}

/** The only presentation knowledge this file needs, and all of it comes from
 *  `kind`: seconds become HH:MM:SS, a code becomes its name, and a capability
 *  the supply has no value for becomes an em dash rather than a number. */
function format(c: PsuCapability): string {
  if (!psuAvailable(c)) return "—"
  if (c.kind === "duration") return formatDuration(c.value)
  if (c.kind === "bool") return c.value !== 0 ? "On" : "Off"
  if (c.kind === "enum") return c.valueLabel ?? String(c.value)
  return `${c.value.toFixed(2)}${c.unit ? ` ${c.unit}` : ""}`
}

function formatDuration(totalSeconds: number): string {
  const s = Math.max(0, Math.floor(totalSeconds))
  const pad = (n: number) => String(n).padStart(2, "0")
  return `${pad(Math.floor(s / 3600))}:${pad(Math.floor((s % 3600) / 60))}:${pad(s % 60)}`
}

// ── Preset ───────────────────────────────────────────────────
//
// Discrete, so a selector — built from the `options` the device sent, which is
// why nothing here maps 0 to "M0". Disabled because recalling a preset rewrites
// every setpoint at once and the firmware does not offer it: showing which one
// is active is honest, offering to change it would not be.

function PresetSelect({ capability }: { capability?: PsuCapability }) {
  if (!capability?.options) return <div />

  const current = capability.valueLabel ?? capability.options[Math.round(capability.value)] ?? "?"

  return (
    <label className="flex items-center gap-2 text-sm">
      <span className="text-muted-foreground">{capability.label}</span>
      <select
        className="h-9 rounded-md border bg-background px-2 font-mono text-sm disabled:opacity-70"
        value={current}
        disabled
        title="Read-only: recalling a preset is not supported yet"
      >
        {capability.options.map((option) => (
          <option key={option} value={option}>
            {option}
          </option>
        ))}
      </select>
    </label>
  )
}

// ── Protections ──────────────────────────────────────────────
//
// The configured thresholds, off the dashboard and behind a button. Editable
// where the device says `rw`, shown as text where it says `r` — so the derived
// limits that would need a multi-register write appear without pretending to be
// settable. Writes go through `psu write`, which validates against the
// capability's own range on the device.

function ProtectionsDialog({
  data,
  write,
}: {
  data: PsuData
  write: (key: string, value: number) => Promise<boolean>
}) {
  const entries = psuGroup(data, "protection")
  if (entries.length === 0) return <div />

  return (
    <Dialog>
      <DialogTrigger asChild>
        <Button variant="outline" size="sm">
          <ShieldIcon className="mr-2 size-4" />
          Protections
        </Button>
      </DialogTrigger>
      <DialogContent className="sm:max-w-md">
        <DialogHeader>
          <DialogTitle>Protections</DialogTitle>
          <DialogDescription>
            Configured trip thresholds, not live values — the dashboard shows the
            supply's current trip state.
          </DialogDescription>
        </DialogHeader>
        <div className="space-y-3">
          {entries.map(([key, c]) => (
            <ProtectionRow key={key} capKey={key} capability={c} write={write} />
          ))}
        </div>
      </DialogContent>
    </Dialog>
  )
}

function ProtectionRow({
  capKey,
  capability,
  write,
}: {
  capKey: string
  capability: PsuCapability
  write: (key: string, value: number) => Promise<boolean>
}) {
  const [draft, setDraft] = useState("")
  const [busy, setBusy] = useState(false)

  async function commit() {
    const value = parseFloat(draft)
    if (isNaN(value)) return
    setBusy(true)
    await write(capKey, value)
    setBusy(false)
    setDraft("")
  }

  return (
    <div className="flex items-center justify-between gap-3">
      <span className="text-sm">{capability.label}</span>
      {capability.access === "rw" ? (
        <div className="flex items-center gap-2">
          <Input
            className="h-9 w-28 text-right font-mono"
            type="number"
            min={capability.min}
            max={capability.max}
            step={0.01}
            placeholder={psuAvailable(capability) ? capability.value.toFixed(2) : "—"}
            value={draft}
            disabled={busy}
            onChange={(e) => setDraft(e.target.value)}
            onKeyDown={(e) => {
              if (e.key === "Enter") commit()
            }}
          />
          <span className="w-8 text-xs text-muted-foreground">{capability.unit}</span>
        </div>
      ) : (
        <span className="font-mono text-sm tabular-nums">{format(capability)}</span>
      )}
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

// A setpoint row is driven entirely by its capability: label, unit, bounds and
// the current value. The editable input IS the displayed setpoint — it used to
// be printed a second time under the label, which was the same number twice. A
// supply without the setpoint renders nothing rather than an input that would
// be refused.
function SetpointRow({
  capability,
  onSet,
}: {
  capability?: PsuCapability
  onSet: (v: number) => void
}) {
  const [value, setValue] = useState("")
  const [editing, setEditing] = useState(false)

  if (!capability) return null

  const { label, unit } = capability
  const current = capability.value
  const min = capability.min ?? 0
  const max = capability.max ?? Number.MAX_SAFE_INTEGER

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
      <div className="text-sm font-medium">{label}</div>
      <div className="flex items-center gap-2">
        <Input
          className="w-28 text-right font-mono"
          type="number"
          min={min}
          max={max}
          step={0.01}
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
