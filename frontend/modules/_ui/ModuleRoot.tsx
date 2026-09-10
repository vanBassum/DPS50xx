// The module/shell style boundary, and the reason it is a boundary rather than a
// convention.
//
// ── What went wrong without it ───────────────────────────────────────────────
// A module used to adopt its stylesheet into the HOST DOCUMENT's <head>. Both a
// module and a shell are Tailwind builds, so both emit the SAME utility class names
// into the SAME `utilities` cascade layer — one namespace, shared by two builds that
// know nothing about each other. Inside a layer the last matching rule of equal
// specificity wins, so which sheet came later decided the look, in both directions:
//
//   - Appending the module's sheet let a module restyle the shell. The shadcn sidebar
//     is display-none by default and block from the md breakpoint up; a module that
//     emitted the display-none utility (a file input does) beat the shell's breakpoint
//     rule and the sidebar vanished at every width. A media query carries no
//     specificity, so nothing broke the tie.
//   - Inserting it FIRST fixed that and created the mirror image: the SHELL now won
//     every class name the two happened to share, and the two shells do not emit the
//     same set. A two-column grid that widens to three at a breakpoint came out three
//     columns in the relay shell, which emits no bare two-column rule, and two on the
//     device shell, which does. Same bundle, same device, different page.
//
// There is no ordering that fixes both, because the problem is not the order — it is
// that the two builds share a namespace at all. So the leak that has to be closed is
// the shared cascade, not the tie-break.
//
// ── What this does ───────────────────────────────────────────────────────────
// Renders the module inside a shadow root and adopts the module's stylesheet THERE.
// Shell rules do not reach in and module rules do not reach out, so a module's
// appearance stops being a function of what its host happened to emit. What still
// crosses the boundary is exactly the contract:
//
//   - CSS custom properties inherit through a shadow boundary, so the shell's shadcn
//     tokens (--background, --primary, --border, --radius, --font-sans, …) arrive
//     unchanged. The shell defines what Strux looks like.
//   - Tailwind emits its own theme variables on `:root, :host`, so the module's scale
//     (--spacing, --text-*, its literal palette) lands on the shadow host and is
//     self-contained. The module defines how its feature UI is built.
//   - Light/dark is a class on a document ancestor, and a descendant selector does not
//     cross a shadow boundary, so the theme is mirrored onto the host element here and
//     `_ui/theme.css` defines `dark` against `:host(.dark)`.
//
// ── Why a portal rather than a second React root ─────────────────────────────
// `createRoot` inside the shadow root would mean shipping a second ReactDOM in every
// module bundle — bytes in a flash partition, and a tree the shell cannot see. A
// portal keeps the module in the shell's React tree while its DOM lives in the shadow
// root, and React attaches its delegated event listeners to a portal's own container
// (`listenToAllSupportedEvents` on HostPortal completion), so events dispatch from
// inside the shadow tree correctly rather than being retargeted to the host element.
//
// `createPortal` is the one thing imported from `react-dom` and it is bundled, not
// external: the shells' import map publishes `react` and `react/jsx-runtime` only, and
// adding a third entry would mean a coordinated change in every shell for a 1.8 KB
// function. Its `require("react")` still resolves to the shared instance through the
// import map, which is the part that actually matters — two Reacts break hooks, two
// copies of react-dom's tiny facade break nothing.
//
// ── Why this is framework-independent ───────────────────────────────────────
// A host does not have to be React, or Tailwind, or shadcn. It has to publish the
// token set on a document ancestor, mark dark mode with a class there, and render
// whatever DOM the module hands it. Everything else the module carries.

import { createPortal } from "react-dom"
import { useEffect, useRef, useState, type ReactNode } from "react"

/// One constructable sheet per distinct stylesheet text, shared by every mount.
///
/// Keyed on the CSS itself rather than a module id: the text IS the identity, a module
/// has exactly one, and it means nothing has to be passed in to keep the cache honest.
/// Re-mounting a page — or re-activating a bundle, which the relay shell does when a
/// device's pipe drops and comes back — reuses the parsed sheet instead of reparsing.
const sheets = new Map<string, CSSStyleSheet>()

function sheetFor(css: string): CSSStyleSheet | null {
  const cached = sheets.get(css)
  if (cached) return cached
  try {
    const sheet = new CSSStyleSheet()
    sheet.replaceSync(css)
    sheets.set(css, sheet)
    return sheet
  } catch {
    // Constructable stylesheets are everywhere this UI runs, but a <style> in the
    // shadow root is the same isolation with none of the sharing, so there is no
    // reason to fail instead.
    return null
  }
}

function attach(host: HTMLElement, css: string): ShadowRoot {
  const shadow = host.shadowRoot ?? host.attachShadow({ mode: "open" })
  const sheet = sheetFor(css)
  if (sheet) {
    if (!shadow.adoptedStyleSheets.includes(sheet))
      shadow.adoptedStyleSheets = [...shadow.adoptedStyleSheets, sheet]
  } else if (!shadow.querySelector("style")) {
    const style = document.createElement("style")
    style.textContent = css
    shadow.appendChild(style)
  }
  return shadow
}

/// Whether the host document is in dark mode, by the two conventions a shadcn shell
/// uses. Deliberately not `prefers-color-scheme`: the shell's tokens follow the
/// shell's own switch, and a module reading the OS instead is how a module ended up
/// light on a dark page.
function documentIsDark(): boolean {
  const roots = [document.documentElement, document.body]
  return roots.some(
    (el) =>
      el != null &&
      (el.classList.contains("dark") || el.getAttribute("data-theme") === "dark"),
  )
}

export function ModuleRoot({ css, children }: { css: string; children: ReactNode }) {
  const hostRef = useRef<HTMLDivElement | null>(null)
  const [shadow, setShadow] = useState<ShadowRoot | null>(null)

  useEffect(() => {
    const host = hostRef.current
    if (!host) return
    setShadow(attach(host, css))
  }, [css])

  // The theme, mirrored from the document onto the host so `dark:` inside the shadow
  // tree means what it means outside it. An observer rather than a one-shot read
  // because a shell's theme toggle mutates that class while the module is mounted.
  useEffect(() => {
    const host = hostRef.current
    if (!host) return

    const sync = () => host.classList.toggle("dark", documentIsDark())
    sync()

    const observer = new MutationObserver(sync)
    const options = { attributes: true, attributeFilter: ["class", "data-theme"] }
    observer.observe(document.documentElement, options)
    if (document.body) observer.observe(document.body, options)
    return () => observer.disconnect()
  }, [])

  return (
    <div
      ref={hostRef}
      // Marks the seam for anyone reading the DOM, and is the only class this element
      // ever carries besides the mirrored theme — it lives in the shell's DOM, where
      // module utilities do not apply.
      className="strux-module-root"
      // `display: contents` so this element generates no box and the module's own root
      // is laid out as though it were the shell's direct child. Not cosmetic: the
      // console page is `h-full` inside the shell's flex column, and a wrapper with
      // auto height is what a percentage height would resolve against — the page would
      // collapse. A boundary that changed the layout would not be a boundary a module
      // could ignore, which is the whole point of putting it here rather than in each
      // page. Inline rather than in the module stylesheet because this element lives in
      // the SHELL's DOM, where the module's sheet does not reach.
      style={{ display: "contents" }}
    >
      {shadow && createPortal(children, shadow)}
    </div>
  )
}
