# neuswc / swc Notes

`inis` targets neuswc first. neuswc is a swc-derived compositor library with a
swc-style public API, so the adapter still includes `swc.h` and calls functions
such as `swc_initialize`, `swc_window_show`, and `swc_window_set_geometry`.

swc is attractive because it is small, written in C, and designed as a library
for simple Wayland compositors and tiling window managers. Its upstream README
describes support for creating a compositor library, XWayland support, and
window borders.

## What swc Provides

- Small C codebase.
- Wayland compositor library shape instead of a full desktop shell.
- DRM/libinput/xkb/pixman/wld-oriented stack.
- Window manager callbacks such as new window and new screen.
- XWayland support in the historical design.
- A philosophy close to `inis`: small, understandable, tiling-friendly.

## What swc Likely Lacks for inis

- A broad modern protocol surface compared with larger compositor frameworks.
- Mature layer-shell support for Waybar-class bars.
- The same volume of test coverage and downstream compositor usage as larger
  compositor stacks.
- Ready-made scene graph, protocol helpers, and damage/render helpers.
- Clear guarantees for current desktop app edge cases.

## Hard Parts With swc

- Layer-shell may need to be implemented or ported: layers, anchors, margins,
  exclusive zones, keyboard interactivity, output assignment, popups, and damage.
- XDG popup correctness must be verified carefully.
- Multi-monitor behavior may require more compositor-side policy than a large
  ready-made framework would.
- XWayland support must be tested against current XWayland expectations.
- Any missing protocol becomes `inis` maintenance burden.

## Why inis Does Not Use wlroots

`inis` deliberately does not use wlroots. wlroots would make many things easier,
but it would also make the backend architecture much larger than the project
wants. The point of `inis` is not to pick the fastest route to a feature-rich
desktop; it is to keep a small C compositor that one person can understand.

The consequence is direct: missing protocol support becomes `inis` work.

## What wlroots Would Have Made Easier

- xdg-shell, layer-shell, XWayland, input, output management, protocol globals,
  and rendering helpers are all widely used by real compositors.
- Waybar support is much more straightforward.
- Debugging common app issues is easier because other wlroots compositors have
  already hit similar problems.

The cost would be dependency size and internal complexity, which is why `inis`
does not take that path.

## Raw wayland-server Path

Raw `wayland-server` plus libinput, DRM/GBM, pixman/OpenGL, xkbcommon, protocol
XML, and explicit render/damage code gives maximum control and minimum hidden
policy. It also means writing years of compositor infrastructure before `inis`
feels useful.

This path best preserves conceptual Unix minimalism, but it is the slowest and
riskiest path to a working desktop.

## Recommendation

Start with a backend-neutral phase-0/phase-1 architecture and prototype neuswc
first. If neuswc is too limited, patch or vendor a small swc-derived backend before
falling all the way back to raw `wayland-server`/DRM/libinput code.

wlroots is intentionally not a fallback for this project.

## Build Probe

```sh
make backend-check
make USE_NEUSWC=1
./inis -d
```

If neuswc is installed outside the default `pkg-config` path:

```sh
PKG_CONFIG_PATH=/opt/neuswc/lib/pkgconfig make USE_NEUSWC=1
```

neuswc currently installs `swc.pc`, so `USE_NEUSWC=1` uses pkg-config module
`swc` but defines `INIS_HAVE_NEUSWC=1`.

If there is no usable `swc.pc`, pass flags manually:

```sh
make USE_NEUSWC=1 NEUSWC_CFLAGS="-I/path/to/neuswc/include" NEUSWC_LIBS="-L/path/to/neuswc/lib -lswc -lwayland-server"
```

The current `backend_swc.c` only checks that a Wayland display can be created
when built with swc dependencies, calls `swc_initialize`, installs the public
`swc_manager` callbacks, then shuts down. It does not yet enter
`wl_display_run`, because `inis` still needs to wire signal handling, dispatch
state, and safe exit first.

## Public API Surface Used First

The first integration targets the documented swc API:

- `swc_initialize(display, event_loop, manager)`
- `swc_finalize()`
- `struct swc_manager`
- `new_screen(struct swc_screen *)`
- `new_window(struct swc_window *)`
- `new_device(struct libinput_device *)`
- `activate()` / `deactivate()`

After that, `inis` can map policy operations onto:

- `swc_window_show`
- `swc_window_hide`
- `swc_window_focus`
- `swc_window_close`
- `swc_window_set_tiled`
- `swc_window_set_stacked`
- `swc_window_set_fullscreen`
- `swc_window_set_geometry`
- `swc_window_set_border`
