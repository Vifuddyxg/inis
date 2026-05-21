# Phase 1 neuswc Backend

This phase exists to answer one question: can `inis` use neuswc as the real backend
without pulling in wlroots?

## Goal

Start `inis` with neuswc, detect screens and input devices, accept new windows, and
exit cleanly.

## Build

```sh
make backend-check
make USE_NEUSWC=1
./inis -d
```

neuswc currently installs `swc.pc`, so `USE_NEUSWC=1` looks for the pkg-config
module `swc` while defining `INIS_HAVE_NEUSWC=1`.

If `swc.pc` from neuswc is not installed, install neuswc or set:

```sh
PKG_CONFIG_PATH=/path/to/neuswc/pkgconfig make USE_NEUSWC=1
```

## Required swc API

- `swc_initialize`
- `swc_finalize`
- `struct swc_manager`
- `swc_screen_set_handler`
- `swc_window_set_handler`
- `swc_window_show`
- `swc_window_hide`
- `swc_window_focus`
- `swc_window_close`
- `swc_window_set_tiled`
- `swc_window_set_stacked`
- `swc_window_set_fullscreen`
- `swc_window_set_geometry`
- `swc_window_set_border`
- `swc_add_binding`

## Work Items

1. Store `struct inis_server *` in backend private state so swc callbacks can
   update the real monitor/window arrays. Done.
2. Convert `new_screen` into `inis_monitor` creation. Done.
3. Convert `new_window` into `inis_window` creation. Done.
4. Install swc screen/window handlers.
5. Map `killactive` to `swc_window_close`. Done in the backend adapter.
6. Map focus to `swc_window_focus`. Done in the backend adapter.
7. Map tiled layout results to `swc_window_set_geometry`. Done in the backend
   adapter.
8. Add a controlled event loop and signal handling. Done in the swc adapter.
9. Implement `exec` without a desktop service. Done with `fork`/`setsid`/`sh -c`.
10. Implement basic workspace switching and moving focused windows. Done for the
    single-monitor MVP model.
11. Register config/default bindings through `swc_add_binding`. Done for a
    small explicit key subset.
12. Expose text IPC through `inisctl`. Done for basic commands and dispatch.
13. Create the Wayland socket with `wl_display_add_socket_auto` and export
    `WAYLAND_DISPLAY`. Done.

## Stop Criteria

Phase 1 is complete when `inis` can start on a TTY, detect an output and input
device, enter the event loop, and exit cleanly without leaking swc state.

Current status: implemented against the local neuswc build installed under
`/tmp/inis-neu-prefix`. More real-session testing is still needed, but the old
pkg-config blocker is gone for local development.
