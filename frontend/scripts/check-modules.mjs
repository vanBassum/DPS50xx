// Build-time guard for the module/shell seam.
//
// The failure this exists for already happened once: `export * from "react"` in the
// host facade compiled, emitted, shipped, and produced a chunk exporting six mangled
// internals instead of React's named bindings. A module's `import { useState } from
// "react"` would have resolved to undefined at runtime, in a browser, on a device —
// and nothing in the build said a word. React is CommonJS, so a star re-export has no
// statically known names to forward.
//
// So this checks the three things that can silently break the seam:
//
//   1. Every bare specifier a module imports is one the shell's import map declares.
//      Anything else is unresolvable in the browser.
//   2. Every NAME a module imports from those specifiers is actually exported by the
//      facade the import map points at.
//   3. No module references `process`. A module runs in a browser, which has no
//      `process` — but Vite does not substitute `process.env.NODE_ENV` in library mode,
//      so a bundled CommonJS dependency ships its own environment switch and throws at
//      import. That is how all four modules died at once with "process is not defined".
//   4. No module writes to the host document's <head>. A module's CSS belongs in its
//      own shadow root (modules/_ui/ModuleRoot.tsx); a stylesheet in the shared <head>
//      puts two independent Tailwind builds in one cascade layer, and then whichever
//      sheet came last decides the look. That failed in both directions before it was
//      closed — a module's `.hidden` deleted the shell's sidebar, and a shell's
//      `.grid-cols-2` overrode a module's `sm:grid-cols-3` — so it is worth a build
//      error rather than a convention.
//
// Run after the module build and before gzip, so it reads plain .js.

import { readdir, readFile } from "fs/promises"
import { existsSync } from "fs"
import { join } from "path"

const www = join(import.meta.dirname, "../../www")
const modulesDir = join(www, "modules")
const assetsDir = join(www, "assets")

// Kept in step with the import map in index.html by hand — there are two entries and
// they change about never. A mismatch here fails the build, which is the point.
const IMPORT_MAP = {
  react: "host-react.js",
  "react/jsx-runtime": "host-jsx-runtime.js",
}

/// Named imports per bare specifier, from ESM source text.
/// Only bare specifiers matter: a relative import inside a module resolves on its own.
function importsBySpecifier(src) {
  const found = new Map()
  const re = /import\s*(?:\{([^}]*)\}|(\w+))?\s*(?:,\s*\{([^}]*)\})?\s*from\s*["']([^"']+)["']/g
  for (const m of src.matchAll(re)) {
    const spec = m[4]
    if (spec.startsWith(".") || spec.startsWith("/")) continue
    const names = found.get(spec) ?? new Set()
    if (m[2]) names.add("default")
    for (const group of [m[1], m[3]]) {
      if (!group) continue
      for (const part of group.split(",")) {
        const name = part.trim().split(/\s+as\s+/)[0].trim()
        if (name) names.add(name)
      }
    }
    found.set(spec, names)
  }
  return found
}

/// Exported names from an ESM facade, covering both `export {a as b}` and
/// `export const x`/`export function x`.
function exportedNames(src) {
  const out = new Set()
  for (const m of src.matchAll(/export\s*\{([^}]*)\}/g)) {
    for (const part of m[1].split(",")) {
      const halves = part.trim().split(/\s+as\s+/)
      const name = (halves[1] ?? halves[0]).trim()
      if (name) out.add(name)
    }
  }
  for (const m of src.matchAll(/export\s+(?:const|let|var|function|class)\s+(\w+)/g))
    out.add(m[1])
  if (/export\s+default\b/.test(src)) out.add("default")
  return out
}

const problems = []

if (!existsSync(modulesDir)) {
  console.log("check-modules: no www/modules — nothing to check")
  process.exit(0)
}

// Resolve each facade once.
const facades = new Map()
for (const [spec, file] of Object.entries(IMPORT_MAP)) {
  const path = join(assetsDir, file)
  if (!existsSync(path)) {
    problems.push(
      `import map points "${spec}" at assets/${file}, which the shell build did not emit`,
    )
    continue
  }
  facades.set(spec, exportedNames(await readFile(path, "utf8")))
}

const entries = (await readdir(modulesDir)).filter((f) => f.endsWith(".js"))
if (entries.length === 0) console.log("check-modules: no module bundles found")

for (const file of entries) {
  const src = await readFile(join(modulesDir, file), "utf8")

  if (!/export\s*\{[^}]*\bactivate\b|export\s+(?:const|function)\s+activate\b/.test(src))
    problems.push(`modules/${file} does not export activate()`)

  if (/\bprocess\s*\.\s*env\b/.test(src))
    problems.push(
      `modules/${file} references process.env, which does not exist in a browser — ` +
        `Vite leaves it alone in library mode, so define it in _ui/vite-module.ts`,
    )

  // Matches the property access however it is spelled after minification: esbuild
  // keeps `document.head` as written, and `document["head"]` is the only alternative.
  if (/document\s*(?:\.\s*head\b|\[\s*["']head["']\s*\])/.test(src))
    problems.push(
      `modules/${file} touches document.head — a module's styles belong in its own ` +
        `shadow root (see modules/_ui/ModuleRoot.tsx), not in the shell's cascade`,
    )

  for (const [spec, names] of importsBySpecifier(src)) {
    const facade = facades.get(spec)
    if (!facade) {
      problems.push(
        `modules/${file} imports "${spec}", which the shell's import map does not declare — ` +
          `it would fail to resolve in the browser`,
      )
      continue
    }
    for (const name of names) {
      if (!facade.has(name))
        problems.push(
          `modules/${file} imports { ${name} } from "${spec}", but assets/${IMPORT_MAP[spec]} ` +
            `does not export it — add it to src/shell/host-${spec === "react" ? "react" : "jsx-runtime"}.js`,
        )
    }
  }
}

if (problems.length) {
  console.error("\ncheck-modules: the module/shell seam is broken\n")
  for (const p of problems) console.error("  - " + p)
  console.error(
    "\nThis check exists because these failures are invisible at build time and fatal at runtime.\n",
  )
  process.exit(1)
}

console.log(
  `check-modules: ${entries.length} module bundle(s) OK — ` +
    `every bare import is declared by the import map and exported by its facade, ` +
    `and none references process or the shell's <head>`,
)
