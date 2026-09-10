/// Turns anything thrown into something showable.
export function errorMessage(error: unknown): string {
  return error instanceof Error ? error.message : String(error)
}

// `adoptStyles` used to live here: it put a module's stylesheet into the HOST
// DOCUMENT's <head>, first child rather than appended, to win a cascade tie against
// the shell. Both directions of that tie were bugs — a module's display-none utility
// deleted the shell's sidebar, and then the shell's grid utilities overrode the
// module's own responsive layout — and no ordering fixed both, because the fault was
// the shared namespace and not the order. A module's styles now go into its own shadow
// root: see ModuleRoot.tsx.
//
// Nothing in a module writes to document.head any more, and scripts/check-modules.mjs
// fails the build if a bundle tries.
