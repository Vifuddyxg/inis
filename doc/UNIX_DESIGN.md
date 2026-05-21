# Unix Design

`inis` uses Unix-style boundaries: the compositor manages windows and outputs;
external tools handle desktop services.

## In Core

- Wayland display
- outputs and monitors
- input devices
- keyboard and pointer focus
- xdg-shell windows
- window management
- workspaces
- layouts
- rendering and damage
- Unix socket IPC

## Outside Core

- bar: Waybar, yambar, sfwbar
- launcher: rofi, wofi, bemenu, fuzzel
- wallpaper: swaybg, wbg
- notifications: mako, fnott
- screenshots: grim and slurp
- clipboard: wl-clipboard
- volume: wpctl, pamixer
- brightness: brightnessctl
- lockscreen: swaylock, gtklock
- network, tray, and settings UI

The compositor should expose enough protocol and IPC surface for those tools to
work, but it should not absorb them.

## IPC

The IPC protocol should be text-first:

```sh
inisctl dispatch workspace 2
inisctl workspaces
inisctl clients
```

JSON is useful for bars and scripts, but text output remains the default.

The first IPC implementation uses one newline-terminated command per Unix socket
connection. This keeps `inisctl` and shell scripts simple, and leaves room for a
long-lived event subscription socket later.
