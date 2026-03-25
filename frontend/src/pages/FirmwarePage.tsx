import { useEffect, useRef, useState } from "react"
import { backend, type UpdateStatus } from "@/lib/backend"
import { useConnectionStatus } from "@/hooks/use-connection-status"
import { useLatestRelease, type ReleaseInfo } from "@/hooks/use-latest-release"
import { isNewerVersion } from "@/lib/version"
import { UploadIcon, ArrowUpCircleIcon, ExternalLinkIcon } from "lucide-react"
import { Button } from "@/components/ui/button"

export default function FirmwarePage() {
  const connection = useConnectionStatus()
  const [status, setStatus] = useState<UpdateStatus | null>(null)
  const release = useLatestRelease()

  useEffect(() => {
    if (connection !== "connected") return
    backend.getUpdateStatus().then(setStatus).catch(() => {})
  }, [connection])

  const updateAvailable = status && release && isNewerVersion(status.firmware, release.version)

  return (
    <div className="mx-auto max-w-2xl space-y-6">
      <h1 className="text-2xl font-bold">Firmware</h1>

      {updateAvailable && release && (
        <UpdateAvailableCard current={status!.firmware} release={release} />
      )}

      {status && (
        <div className="rounded-xl border bg-card p-6 text-card-foreground shadow-sm">
          <h2 className="mb-3 text-lg font-semibold">Current Status</h2>
          <div className="grid grid-cols-2 gap-x-8 gap-y-2 text-sm">
            <Row label="Version" value={status.firmware} />
            <Row label="Running" value={status.running} />
            <Row label="Next slot" value={status.nextSlot} />
            {release && !updateAvailable && (
              <Row label="Latest" value={`${release.version} (up to date)`} />
            )}
          </div>
        </div>
      )}

      <UploadCard
        title="Application Firmware"
        description="Upload a .bin file to update the application firmware. The device will reboot after upload."
        accept=".bin"
        onUpload={(file, onProgress) => backend.uploadFirmware(file, onProgress)}
      />

      <UploadCard
        title="WWW Partition"
        description="Upload a FAT image to update the web interface. The device will reboot after upload."
        accept=".bin"
        onUpload={(file, onProgress) => backend.uploadWww(file, onProgress)}
      />
    </div>
  )
}

function UpdateAvailableCard({ current, release }: { current: string; release: ReleaseInfo }) {
  return (
    <div className="rounded-xl border border-emerald-500/30 bg-emerald-500/5 p-6 shadow-sm">
      <div className="flex items-center gap-2">
        <ArrowUpCircleIcon className="size-5 text-emerald-500" />
        <h2 className="text-lg font-semibold">Update Available</h2>
      </div>
      <p className="mt-1 text-sm text-muted-foreground">
        Version <span className="font-mono font-medium text-foreground">{release.version}</span> is available
        (you have <span className="font-mono">{current}</span>).
        Download the binaries from GitHub and upload them below.
      </p>
      <div className="mt-3">
        <Button variant="outline" size="sm" asChild>
          <a href={release.url} target="_blank" rel="noopener noreferrer">
            Go to release
            <ExternalLinkIcon className="ml-1.5 size-3" />
          </a>
        </Button>
      </div>
    </div>
  )
}

function Row({ label, value }: { label: string; value: string }) {
  return (
    <div className="flex justify-between">
      <span className="text-muted-foreground">{label}</span>
      <span className="font-mono">{value}</span>
    </div>
  )
}

function UploadCard({
  title,
  description,
  accept,
  onUpload,
}: {
  title: string
  description: string
  accept: string
  onUpload: (
    file: File,
    onProgress: (percent: number) => void,
  ) => Promise<unknown>
}) {
  const fileRef = useRef<HTMLInputElement>(null)
  const [file, setFile] = useState<File | null>(null)
  const [progress, setProgress] = useState<number | null>(null)
  const [error, setError] = useState<string | null>(null)

  const uploading = progress !== null

  async function handleUpload() {
    if (!file) return
    setError(null)
    setProgress(0)

    try {
      await onUpload(file, setProgress)
    } catch (e) {
      setError(e instanceof Error ? e.message : "Upload failed")
      setProgress(null)
    }
  }

  return (
    <div className="rounded-xl border bg-card p-6 text-card-foreground shadow-sm">
      <div className="mb-2 flex items-center gap-2">
        <UploadIcon className="size-5 text-muted-foreground" />
        <h2 className="text-lg font-semibold">{title}</h2>
      </div>
      <p className="mb-4 text-sm text-muted-foreground">{description}</p>

      <div className="flex items-center gap-3">
        <input
          ref={fileRef}
          type="file"
          accept={accept}
          disabled={uploading}
          className="text-sm file:mr-3 file:rounded-lg file:border-0 file:bg-primary file:px-3 file:py-1.5 file:text-sm file:font-medium file:text-primary-foreground hover:file:bg-primary/80 disabled:opacity-50"
          onChange={(e) => {
            setFile(e.target.files?.[0] ?? null)
            setError(null)
          }}
        />
        <Button
          onClick={handleUpload}
          disabled={!file || uploading}
          size="sm"
        >
          {uploading ? `${progress}%` : "Upload"}
        </Button>
      </div>

      {file && !uploading && (
        <p className="mt-2 text-xs text-muted-foreground">
          {file.name} ({(file.size / 1024).toFixed(1)} KB)
        </p>
      )}

      {uploading && (
        <div className="mt-3 h-2 overflow-hidden rounded-full bg-muted">
          <div
            className="h-full bg-primary transition-all"
            style={{ width: `${progress}%` }}
          />
        </div>
      )}

      {error && (
        <p className="mt-2 text-sm text-red-500">{error}</p>
      )}
    </div>
  )
}
