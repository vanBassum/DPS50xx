import { readdir, readFile, writeFile, unlink } from "fs/promises"
import { extname, join } from "path"
import { gzipSync } from "zlib"

const wwwDir = join(import.meta.dirname, "../../www")

// Formats that are already compressed, and gzipping them is worse than pointless: a
// woff2 came out 28 bytes LARGER, and the device would then serve it with
// Content-Encoding: gzip so the browser pays a decompress pass to recover the bytes it
// could have had directly. The font subsets shadcn's preset brought in are the first
// binary assets in www; before them every file here was text and this needed no list.
const PRECOMPRESSED = new Set([".woff2", ".woff", ".png", ".jpg", ".jpeg", ".webp", ".avif"])

async function gzipDir(dir) {
  const entries = await readdir(dir, { withFileTypes: true })
  for (const entry of entries) {
    const fullPath = join(dir, entry.name)
    if (entry.isDirectory()) {
      await gzipDir(fullPath)
    } else if (entry.name.endsWith(".gz")) {
      continue
    } else if (PRECOMPRESSED.has(extname(entry.name).toLowerCase())) {
      console.log(`${entry.name} → left as-is (already compressed)`)
    } else {
      const data = await readFile(fullPath)
      const compressed = gzipSync(data, { level: 9 })
      await writeFile(fullPath + ".gz", compressed)
      await unlink(fullPath)
      const pct = ((1 - compressed.length / data.length) * 100).toFixed(0)
      console.log(`${entry.name} → ${entry.name}.gz (${data.length} → ${compressed.length}, -${pct}%)`)
    }
  }
}

await gzipDir(wwwDir)
