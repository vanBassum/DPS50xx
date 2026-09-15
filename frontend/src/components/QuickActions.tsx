import { PowerIcon } from "lucide-react"
import { toast } from "sonner"
import {
  AlertDialog,
  AlertDialogAction,
  AlertDialogCancel,
  AlertDialogContent,
  AlertDialogDescription,
  AlertDialogFooter,
  AlertDialogHeader,
  AlertDialogTitle,
  AlertDialogTrigger,
} from "@/components/ui/alert-dialog"
import {
  SidebarGroup,
  SidebarGroupContent,
  SidebarGroupLabel,
  SidebarMenu,
  SidebarMenuButton,
  SidebarMenuItem,
} from "@/components/ui/sidebar"
import { backend } from "@/lib/backend"

function errorMessage(e: unknown): string {
  return e instanceof Error ? e.message : "Unknown error"
}

// A quick action is a whole-device verb — it acts on the device, not on the page
// you happen to be looking at, so it does not belong to any one page. Reboot was
// only reachable from Settings, which made restarting the supply a navigation
// problem: you went to a page you did not want in order to press a button that
// has nothing to do with settings.
//
// `confirm` is what separates an action from a link. Anything that interrupts the
// device gets one; a read-only action would simply omit it and fire on click.
interface QuickAction {
  key: string
  title: string
  icon: typeof PowerIcon
  /** Destructive styling AND a confirmation step — the two travel together. */
  confirm?: { title: string; description: string; action: string }
  run: () => Promise<unknown>
  /** Shown on success. A reboot never reports back, so this is the only feedback. */
  toast: { title: string; description: string }
}

const actions: QuickAction[] = [
  {
    key: "reboot",
    title: "Reboot",
    icon: PowerIcon,
    confirm: {
      title: "Reboot the device?",
      description:
        "The device restarts and drops this connection for a few seconds. " +
        "Unsaved settings changes will be lost.",
      action: "Reboot",
    },
    run: () => backend.reboot(),
    toast: {
      title: "Rebooting device…",
      description: "The connection will drop for a few seconds.",
    },
  },
]

function runAction(a: QuickAction) {
  a.run()
    .then(() => toast.info(a.toast.title, { description: a.toast.description }))
    .catch((e) => toast.error(`${a.title} failed`, { description: errorMessage(e) }))
}

/** The sidebar's bottom group: device-wide verbs, reachable from every page. */
export function QuickActions() {
  return (
    <SidebarGroup className="mt-auto">
      <SidebarGroupLabel>Quick actions</SidebarGroupLabel>
      <SidebarGroupContent>
        <SidebarMenu>
          {actions.map((a) => {
            const button = (
              <SidebarMenuButton
                className={a.confirm ? "text-destructive hover:text-destructive" : undefined}
                onClick={a.confirm ? undefined : () => runAction(a)}
              >
                <a.icon />
                <span>{a.title}</span>
              </SidebarMenuButton>
            )

            return (
              <SidebarMenuItem key={a.key}>
                {a.confirm ? (
                  <AlertDialog>
                    <AlertDialogTrigger asChild>{button}</AlertDialogTrigger>
                    <AlertDialogContent>
                      <AlertDialogHeader>
                        <AlertDialogTitle>{a.confirm.title}</AlertDialogTitle>
                        <AlertDialogDescription>{a.confirm.description}</AlertDialogDescription>
                      </AlertDialogHeader>
                      <AlertDialogFooter>
                        <AlertDialogCancel>Cancel</AlertDialogCancel>
                        <AlertDialogAction onClick={() => runAction(a)}>
                          {a.confirm.action}
                        </AlertDialogAction>
                      </AlertDialogFooter>
                    </AlertDialogContent>
                  </AlertDialog>
                ) : (
                  button
                )}
              </SidebarMenuItem>
            )
          })}
        </SidebarMenu>
      </SidebarGroupContent>
    </SidebarGroup>
  )
}
