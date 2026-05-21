# Performance

`inis` must be event-driven. It should not have a permanent render loop, a
permanent animation loop, or global repaint on every input event.

## Idle Test

Run `inis`, open a terminal, stop typing, and watch `top` or `htop`.
Compositor CPU should be close to 0%. Render logs should stop when nothing
changes.

Use `inisctl damage` to inspect compositor-side damage counters. When idle,
`pending` should stay `no` and `flushes` should not increase repeatedly.

## Terminal Typing Test

Open Alacritty, kitty, and foot. Type quickly. Only the terminal surface damage
should repaint. The whole output should not repaint for every keypress unless
the backend cannot expose finer damage yet.

## Launcher Test

Open rofi, wofi, bemenu, or fuzzel. Type quickly and close it. There should be
no freeze, focus loss, or popup tiling bug.

## Browser Test

Open Firefox or LibreWolf. Test menus, context menus, downloads, dialogs, and
picture-in-picture. Popups must not be tiled. Floating rules should apply
deterministically.

## Workspace Test

Switch workspaces rapidly and move windows between them. Hidden workspaces must
not render. Damage should be limited to affected outputs.

## Mouse Test

Move and resize floating windows. Damage old and new regions. Pointer movement
alone should not trigger full redraw.

## Bar Test

When layer-shell exists, run Waybar. Exclusive zones should reduce usable monitor
area. Bar updates should not repaint all client windows.

## Damage Tracking Architecture

`inis` maintains two levels of damage tracking:

1. **Global damage** (`server->damage`): union of all pending dirty rectangles
   across all outputs. Tracks total events, flushes, and skipped flushes.

2. **Per-output damage** (`monitor->damage`): damage intersected with each
   output's geometry. When a window moves entirely on one output, only that
   output's damage is set. Multi-output spanning rectangles update all affected
   outputs.

`inisctl damage` shows both levels:

```
global pending:yes bounds:640x480+0+0 events:5 flushes:3 skipped:0 reason:window-move
output:eDP-1 pending:yes bounds:640x480+0+0 events:3 flushes:1 skipped:0 reason:window-move
output:HDMI-A-1 pending:no bounds:0x0+0+0 events:2 flushes:2 skipped:1 reason:-
```

When no outputs are connected: `no outputs`.

`inis_render_flush_damage()` in `render.c` clears both the global damage and
each output's pending damage atomically. Skipped flushes (no pending damage)
increment both global and per-output skipped counters.

The render path (`inis_render_output`) is called per-output only when that
output has pending damage. This avoids waking the GPU for outputs that were
not touched.

### Relationship with neuswc/swc backend

The neuswc backend event loop runs via `wl_display_run()`. Surface damage is
signalled by client commits → swc routes geometry changes → `swc_new_screen`,
`swc_new_window`, and window handler callbacks call `inis_server_add_monitor`,
`inis_server_add_window`, etc. → each of these calls `inis_server_mark_damage`
→ damage propagates to both global and per-output structs → `inis_server_flush_damage`
is called → `inis_render_flush_damage` clears damage and logs the flush.

The compositor does not hold a permanent render loop. It only repaints when damage
is pending. Idle CPU should be near zero.

## Debug Tools

- `WAYLAND_DEBUG=1 app`: inspect client protocol traffic.
- `top` / `htop`: idle CPU checks.
- `perf top`: hotspots under load.
- `strace`: only for startup, file/socket issues, or blocking syscalls.
- `inisctl damage`: per-output damage counters in real time.
- `inisctl backend`: backend type and running state.
- compositor logs: state transitions and backend events.
- render damage logs: verify repaint scope.
- frame callback logs: verify callbacks stop when idle.
