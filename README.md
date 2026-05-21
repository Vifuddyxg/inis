# inis

`inis` is planned as a Unix-style Wayland compositor with a Hyprland-like
workflow: familiar keybinds, dispatchers, workspaces, floating/fullscreen
behavior, special workspace support, and text IPC through `inisctl`.

The internal goal is smaller: a readable C compositor core that delegates bars,
launchers, wallpaper, notifications, screenshots, clipboard management, volume,
brightness, and locking to external tools.

Current state: early working compositor core. It builds, starts, logs,
initializes core state, exposes text IPC, and has a neuswc event-loop adapter
with monitor/window callbacks, keybind registration, basic tiling, focus
cycling, workspace movement, simple config variables, `exec-once`, mouse
move/resize hooks, and a first special workspace/scratchpad model. It is not
yet a complete daily-driver compositor. wlroots is not used.

## Build

```sh
make
./inis -d
./inis --features
```

## Login Session

Install the greeter/display-manager session entry:

```sh
USE_NEUSWC=1 ./install-inis-session.sh
```

Then select `inis` in the login manager. The session wrapper logs to
`~/.local/state/inis/session.log`.

For a temporary neuswc stack built by `build-neuswc.sh`:

```sh
sudo make install-neuswc-local
```

For a cleaner greeter setup, install neuwld/neuswc into `/usr/local`, then:

```sh
sudo make USE_NEUSWC=1 install
```

Plain `sudo make install` intentionally refuses to install a greeter session
without a Wayland backend.

Probe backend dependencies:

```sh
./build-neuswc.sh
make backend-check
make USE_NEUSWC=1
./inis -d
```

IPC examples:

```sh
inisctl workspaces
inisctl clients
inisctl monitors
inisctl activewindow
inisctl damage
inisctl dispatch workspace 2
inisctl dispatch workspace previous
inisctl dispatch togglespecialworkspace magic
inisctl dispatch exec alacritty
```

Set `INIS_SOCKET=/path/to/socket` if the compositor is not using the default
`$XDG_RUNTIME_DIR/inis-0.sock`.

## Design Rules

- C11 by default, no C++, no Rust.
- No GLib, Qt, Electron, JavaScript, or Lua runtime in the compositor core.
- No systemd dependency in the core.
- No DBus dependency in the core unless a later feature forces it.
- No bar, launcher, wallpaper renderer, notification daemon, lockscreen, or
  settings UI in core.
- No plugin runtime in MVP.
- No blur, shadows, animated borders, default opacity, or permanent render loop.
- Rendering must be event-driven and damage-based.

## Planned Commands

```sh
inisctl dispatch workspace 2
inisctl dispatch exec alacritty
inisctl dispatch togglefloating
inisctl clients
inisctl monitors
inisctl workspaces
inisctl activewindow
```

See `doc/` for the design, roadmap, config format, performance checklist, and
swc notes.

The immediate backend work is tracked in `doc/PHASE1_SWC.md`.
The local neuswc build recipe is in `doc/NEUSWC_BUILD.md`.
