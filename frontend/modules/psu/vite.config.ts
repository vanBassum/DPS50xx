import { moduleConfig } from "../_ui/vite-module"

// The supply module: this product's own feature, and the reason the firmware exists.
// It is declared FIRST by the firmware (see PsuManager's uiPages_), so it is the page
// a shell lands on — the framework's console/settings/firmware modules come after it.
export default moduleConfig(import.meta.dirname, "psu")
