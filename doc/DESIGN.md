# Design

`inis` is a compositor, not a desktop environment. The core owns only the pieces
that must be inside a Wayland compositor: display/server lifecycle, outputs,
input, focus, windows, workspaces, layout, rendering, damage, and IPC.

## Core Structures

- `inis_server`: top-level owner for backend state, monitors, workspaces,
  windows, focus, event loop, and shutdown.
- `inis_backend`: future wrapper around swc or raw backend code. wlroots is not
  used by this project.
- `inis_monitor`: output geometry, scale, refresh, usable area, active
  workspace, previous workspace, and output-local damage.
- `inis_window`: xdg toplevel state, app id, title, tiled/floating geometry,
  focus state, fullscreen/floating state, workspace and monitor ownership.
- `inis_workspace`: normal or special workspace metadata and window membership.
- `inis_layout`: pure rectangle calculation.
- `inis_binding`: parsed key or mouse binding.
- `inis_dispatcher`: name plus function pointer shared by keybinds and IPC.
- `inis_rule`: future ordered match/action rule.
- `inis_damage`: future per-output damaged regions.

## Event Flow

1. Backend receives a Wayland, DRM, input, or timer event.
2. The event updates explicit compositor state.
3. State changes mark precise damage and schedule output repaint if needed.
4. Repaint happens only for outputs with damage.
5. Frame callbacks are sent for surfaces actually presented.

Idle means no timers driving frames and no repeated rendering.

## Input Flow

Keyboard events update modifier state, match bindings, and call dispatchers.
Unmatched keyboard events go to the focused client. Pointer motion updates
pointer focus and only damages output while a window is being moved or resized.
Plain pointer motion must not repaint the world.

## Render Flow

Rendering walks visible layer surfaces, tiled/floating windows, popups, borders,
and cursor state for one output. Layout does not render. Window management does
not render directly. They mark damage and let the render path consume it.

## Damage Flow

Damage is per output. Window movement damages old and new rectangles. Focus
changes damage border rectangles. Client commits damage the committed surface
region. Workspace switches damage the affected output, not all outputs.

## Workspace Flow

Each monitor has an active normal workspace. A special workspace is an overlay
scratchpad and does not permanently replace the monitor's normal workspace.
Moving a window updates ownership, recalculates layout on source and target
workspaces, and damages affected outputs.

## Focus Flow

Focus is explicit: one focused monitor, one focused window, and a focus stack per
workspace later. Popups are not normal windows and must not be tiled.

## Dispatcher Flow

Keybinds and IPC both call:

```c
struct inis_dispatcher {
	const char *name;
	void (*run)(struct inis_server *server, const char *args);
};
```

Dispatchers are intentionally plain C functions. They should update compositor
state, request configure/close operations, and mark damage. They should not parse
the whole config file or render directly.

## IPC Flow

`inisctl` will connect to a Unix socket and send newline-terminated text
commands. Text output is default. JSON is optional. DBus is not part of the core
IPC design.
