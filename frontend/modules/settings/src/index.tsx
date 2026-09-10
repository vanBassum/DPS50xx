// The settings module. Framework, not application: `settings list` exists on every
// Strux device and describes itself, so the page is generated from a declaration and
// belongs with the framework — registered by SettingsManager, which owns the commands.

import css from "./index.css?inline"
import type { ActivateFn } from "@shell/contract"
import { ModuleRoot } from "../../_ui"
import { SettingsPage } from "./SettingsPage"

export const activate: ActivateFn = (shell) => {
  shell.routes.register({
    id: "settings",
    render: () => (
      <ModuleRoot css={css}>
        <SettingsPage shell={shell} />
      </ModuleRoot>
    ),
  })
}
