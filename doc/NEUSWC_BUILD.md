# Building neuswc for inis

`inis` targets neuswc first. neuswc currently installs a pkg-config file named
`swc.pc`, because its public API remains swc-compatible.

## Recommended Local Prefix

The stable prefix for local development is `~/.local/inis-neu-prefix`.
The helper script defaults to this location:

```sh
./build-neuswc.sh
```

This clones/updates neuwld and neuswc, builds them, and installs into
`~/.local/inis-neu-prefix`.

Then build inis with rpath embedded (no LD_LIBRARY_PATH needed at runtime):

```sh
PKG_CONFIG_PATH=$HOME/.local/inis-neu-prefix/lib64/pkgconfig:$HOME/.local/inis-neu-prefix/lib/pkgconfig \
NEUSWC_PREFIX=$HOME/.local/inis-neu-prefix \
make USE_NEUSWC=1
```

Verify:

```sh
./inis --features
```

Expected:

```
inis 0.0.0-dev
neuswc=yes
swc=yes
ipc=yes
wlroots=no
```

The binary embeds an rpath pointing to `NEUSWC_PREFIX/lib` and `NEUSWC_PREFIX/lib64`,
so no `LD_LIBRARY_PATH` is required after install.

## If neuswc is already built at /tmp/inis-neu-prefix

Override the prefix:

```sh
PKG_CONFIG_PATH=/tmp/inis-neu-prefix/lib64/pkgconfig:/tmp/inis-neu-prefix/lib/pkgconfig \
NEUSWC_PREFIX=/tmp/inis-neu-prefix \
make USE_NEUSWC=1
```

If `/tmp/neuwld` and `/tmp/neuswc` already exist and you want to rebuild without
network access:

```sh
NO_UPDATE=1 NEU_PREFIX=/tmp/inis-neu-prefix ./build-neuswc.sh
```

## Manual steps

```sh
git clone https://git.sr.ht/~shrub900/neuwld /tmp/neuwld
git clone https://git.sr.ht/~shrub900/neuswc /tmp/neuswc
PREFIX=$HOME/.local/inis-neu-prefix

cd /tmp/neuwld
meson setup build --prefix=$PREFIX
meson compile -C build
meson install -C build

cd /tmp/neuswc
PKG_CONFIG_PATH=$PREFIX/lib64/pkgconfig meson setup build --prefix=$PREFIX
PKG_CONFIG_PATH=$PREFIX/lib64/pkgconfig meson compile -C build
PKG_CONFIG_PATH=$PREFIX/lib64/pkgconfig meson install -C build

cd ~/inis
PKG_CONFIG_PATH=$PREFIX/lib64/pkgconfig:$PREFIX/lib/pkgconfig \
NEUSWC_PREFIX=$PREFIX \
make USE_NEUSWC=1
./inis --features
```

## Notes

- neuswc depends on neuwld, which provides `wld.pc`.
- neuswc provides `swc.pc`, not `neuswc.pc`.
- `USE_NEUSWC=1` looks for pkg-config module `swc`, but defines `INIS_HAVE_NEUSWC=1`.
- The binary embeds `-Wl,-rpath,$(NEUSWC_PREFIX)/lib` so it finds libswc at runtime
  without LD_LIBRARY_PATH, as long as NEUSWC_PREFIX is correct at build time.
- Running the compositor still needs a real TTY/session environment, seat/input
  permissions, and the usual DRM/KMS access via swc-launch.

## Installing inis

For a system install where neuwld/neuswc are in `/usr/local`:

```sh
sudo make USE_NEUSWC=1 install
```

For a local neuswc prefix install (embeds rpath to prefix):

```sh
PKG_CONFIG_PATH=$HOME/.local/inis-neu-prefix/lib64/pkgconfig:$HOME/.local/inis-neu-prefix/lib/pkgconfig \
NEUSWC_PREFIX=$HOME/.local/inis-neu-prefix \
sudo -E make USE_NEUSWC=1 install
```

Or use the shortcut target (uses NEUSWC_PREFIX default = ~/.local/inis-neu-prefix):

```sh
sudo -E make install-neuswc-local
```

After install:

```sh
command -v inis
inis --features
```

The installed binary will show `neuswc=yes` and find libswc via the embedded rpath.

## Wayland Session

The session file `/usr/share/wayland-sessions/inis.desktop` runs `inis-session`.
`inis-session` uses the installed compositor binary, so update it with
`sudo make install`. It automatically sets `LD_LIBRARY_PATH` from
`~/.local/inis-neu-prefix` (or `$INIS_NEU_PREFIX` if set). If libswc is in
the rpath, this is a no-op.

To verify the installed binary before starting a session:

```sh
inis --features   # must show neuswc=yes
```

If it shows `neuswc=no`, the wrong binary was installed. Reinstall with `USE_NEUSWC=1`.
