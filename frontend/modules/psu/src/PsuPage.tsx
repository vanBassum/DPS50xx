import { useState } from "react"
import type { ShellProvider } from "@shell/contract"
import { Button, Input, Modal, Panel } from "../../_ui"
import {
  format,
  psuAvailable,
  psuCapability,
  psuFlag,
  psuGroup,
  psuNumber,
  usePsu,
  PSU_KEYS,
  type PsuCapability,
  type PsuData,
} from "./psu"

// This page consumes CAPABILITIES, never registers. It addresses a handful by
// meaning (PSU_KEYS: what it reads out, edits and toggles) and renders the rest from
// the `group` and `kind` the driver sent — so a supply with values this build has
// never heard of still displays, and nothing here knows that an XY6020L keeps its
// runtime in three registers or calls an absent probe 8888.
//
// No icons: a module bundles everything it imports, and the shell's import map
// declares only React, so a lucide import here would be bytes in flash and a
// check-modules failure. The page says what it means in words instead.

export function PsuPage({ shell }: { shell: ShellProvider }) {
  const { data, error, write, setVoltage, setCurrent, setOutput, setKeyLock } = usePsu(
    shell.transport,
  )

  return (
    <div className="mx-auto max-w-2xl space-y-4">
      <div className="flex items-center justify-between gap-3">
        <div>
          <h1 className="text-2xl font-bold">Supply</h1>
          <p className="text-muted-foreground text-sm">{shell.device.name}</p>
        </div>
        {data && (
          <span
            className={`rounded-full px-2.5 py-0.5 text-xs font-medium ${
              data.online
                ? "bg-emerald-500/15 text-emerald-600 dark:text-emerald-500"
                : "bg-destructive/15 text-destructive"
            }`}
          >
            {data.online ? "Online" : "Offline"}
          </span>
        )}
      </div>

      {!data && (
        <Panel className="text-muted-foreground p-8 text-center">
          Reading the supply…
        </Panel>
      )}

      {data && !data.online && (
        <Panel className="p-8 text-center text-amber-600 dark:text-amber-500">
          Supply not responding on Modbus. Check wiring and power.
        </Panel>
      )}

      {error && (
        <div className="border-destructive/20 bg-destructive/5 text-destructive rounded-lg border px-4 py-2.5 text-sm">
          {error}
        </div>
      )}

      {data?.online && (
        <>
          {/* Live readouts */}
          <div className="grid grid-cols-3 gap-3">
            <Readout label="Voltage" unit="V" value={psuNumber(data, PSU_KEYS.outputVoltage)} />
            <Readout label="Current" unit="A" value={psuNumber(data, PSU_KEYS.outputCurrent)} />
            <Readout label="Power" unit="W" value={psuNumber(data, PSU_KEYS.outputPower)} />
          </div>

          {/* Status bar */}
          <Panel className="flex items-center justify-between px-4 py-2.5">
            <StatusItem label="Input" value={`${psuNumber(data, PSU_KEYS.inputVoltage).toFixed(1)}V`} />
            <Divider />
            <StatusItem
              label="Mode"
              value={psuFlag(data, PSU_KEYS.constantCurrent) ? "CC" : "CV"}
              highlight={psuFlag(data, PSU_KEYS.constantCurrent)}
            />
            <Divider />
            {/* The supply names its own protection codes, so there is no label table
                on this side to fall out of date. This is the current TRIP STATE — the
                configured thresholds are behind Protections. */}
            <StatusItem
              label="Protection"
              value={psuCapability(data, PSU_KEYS.protectionState)?.valueLabel ?? "?"}
              highlight={psuNumber(data, PSU_KEYS.protectionState) !== 0}
            />
          </Panel>

          {/* Controls */}
          <Panel className="space-y-5 p-6">
            <SetpointRow capability={psuCapability(data, PSU_KEYS.setVoltage)} onSet={setVoltage} />
            <SetpointRow capability={psuCapability(data, PSU_KEYS.setCurrent)} onSet={setCurrent} />

            <div className="flex items-center gap-3 pt-2">
              <Button
                className={`h-12 flex-1 text-base font-bold ${
                  psuFlag(data, PSU_KEYS.outputEnabled)
                    ? "bg-emerald-600 text-white hover:bg-emerald-700"
                    : "bg-red-600 text-white hover:bg-red-700"
                }`}
                onClick={() => setOutput(!psuFlag(data, PSU_KEYS.outputEnabled))}
              >
                {psuFlag(data, PSU_KEYS.outputEnabled) ? "OUTPUT ON" : "OUTPUT OFF"}
              </Button>

              {psuCapability(data, PSU_KEYS.keyLock) && (
                <Button
                  variant="outline"
                  className="h-12 w-24 shrink-0"
                  onClick={() => setKeyLock(!psuFlag(data, PSU_KEYS.keyLock))}
                  title={psuFlag(data, PSU_KEYS.keyLock) ? "Unlock the panel keys" : "Lock the panel keys"}
                >
                  {psuFlag(data, PSU_KEYS.keyLock) ? "Locked" : "Unlocked"}
                </Button>
              )}
            </div>

            <div className="border-border flex items-center justify-between gap-3 border-t pt-4">
              <PresetSelect capability={psuCapability(data, PSU_KEYS.activePreset)} />
              <Protections data={data} write={write} />
            </div>
          </Panel>

          <SessionPanel data={data} />
        </>
      )}
    </div>
  )
}

// ── Session ──────────────────────────────────────────────────
//
// Whatever the driver grouped as `session`: charge, energy, runtime, temperatures.
// No key list here — a supply that accumulates something else shows it without this
// component changing.

function SessionPanel({ data }: { data: PsuData }) {
  const entries = psuGroup(data, "session")
  if (entries.length === 0) return null

  return (
    <Panel>
      <div className="text-muted-foreground mb-3 text-xs font-medium tracking-wide uppercase">
        Session
      </div>
      <div className="grid grid-cols-2 gap-x-6 gap-y-2 sm:grid-cols-3">
        {entries.map(([key, c]) => (
          <div key={key} className="flex items-baseline justify-between gap-2">
            <span className="text-muted-foreground text-xs">{c.label}</span>
            <span className="font-mono text-sm font-medium tabular-nums">{format(c)}</span>
          </div>
        ))}
      </div>
    </Panel>
  )
}

// ── Preset ───────────────────────────────────────────────────
//
// Discrete, so a selector — built from the `options` the device sent, which is why
// nothing here maps 0 to "M0". Disabled because recalling a preset rewrites every
// setpoint at once and the firmware does not offer it: showing which one is active is
// honest, offering to change it would not be.

function PresetSelect({ capability }: { capability?: PsuCapability }) {
  if (!capability?.options) return <div />

  const current = capability.valueLabel ?? capability.options[Math.round(capability.value)] ?? "?"

  return (
    <label className="flex items-center gap-2 text-sm">
      <span className="text-muted-foreground">{capability.label}</span>
      <select
        className="border-input h-8 rounded-lg border bg-transparent px-2 font-mono text-sm disabled:opacity-70"
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
// The configured thresholds, off the dashboard and behind a button. Editable where
// the device says `rw`, shown as text where it says `r` — so the derived limits that
// would need a multi-register write appear without pretending to be settable. Writes
// go through `psu write`, which validates against the capability's own range on the
// device.

function Protections({
  data,
  write,
}: {
  data: PsuData
  write: (key: string, value: number) => Promise<boolean>
}) {
  const [open, setOpen] = useState(false)
  const entries = psuGroup(data, "protection")
  if (entries.length === 0) return <div />

  return (
    <>
      <Button variant="outline" size="sm" onClick={() => setOpen(true)}>
        Protections
      </Button>
      <Modal
        open={open}
        onClose={() => setOpen(false)}
        title="Protections"
        footer={
          <Button variant="outline" onClick={() => setOpen(false)}>
            Close
          </Button>
        }
      >
        <p className="text-muted-foreground mb-4 text-xs">
          Configured trip thresholds, not live values — the dashboard shows the
          supply's current trip state.
        </p>
        <div className="space-y-3">
          {entries.map(([key, c]) => (
            <ProtectionRow key={key} capKey={key} capability={c} write={write} />
          ))}
        </div>
      </Modal>
    </>
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
            className="w-28 text-right font-mono"
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
          <span className="text-muted-foreground w-8 text-xs">{capability.unit}</span>
        </div>
      ) : (
        <span className="font-mono text-sm tabular-nums">{format(capability)}</span>
      )}
    </div>
  )
}

// ── Subcomponents ────────────────────────────────────────────

function Readout({ label, value, unit }: { label: string; value?: number; unit: string }) {
  return (
    <Panel className="p-4 text-center">
      <div className="text-3xl font-bold tabular-nums">
        {value !== undefined ? value.toFixed(2) : "--.-"}
      </div>
      <div className="text-muted-foreground mt-1 text-xs">
        {label} ({unit})
      </div>
    </Panel>
  )
}

// A setpoint row is driven entirely by its capability: label, unit, bounds and the
// current value. The editable input IS the displayed setpoint — it used to be printed
// a second time under the label, which was the same number twice. A supply without
// the setpoint renders nothing rather than an input that would be refused.
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
  const min = capability.min ?? 0
  const max = capability.max ?? Number.MAX_SAFE_INTEGER

  function handleSubmit() {
    const num = parseFloat(value)
    if (!isNaN(num) && num >= min && num <= max) onSet(num)
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
          placeholder={capability.value.toFixed(2)}
          value={value}
          onChange={(e) => {
            setValue(e.target.value)
            setEditing(true)
          }}
          onKeyDown={(e) => {
            if (e.key === "Enter") handleSubmit()
          }}
        />
        <span className="text-muted-foreground w-4 text-sm">{unit}</span>
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
      <div className="text-muted-foreground text-xs">{label}</div>
      <div
        className={`font-mono text-sm font-medium ${
          highlight ? "text-amber-600 dark:text-amber-500" : ""
        }`}
      >
        {value}
      </div>
    </div>
  )
}

function Divider() {
  return <div className="bg-border h-8 w-px" />
}
