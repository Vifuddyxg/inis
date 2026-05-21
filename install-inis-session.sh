#!/bin/sh
set -eu

cd "$(dirname "$0")"
make USE_NEUSWC="${USE_NEUSWC:-1}"

sudo install -Dm755 inis /usr/local/bin/inis
sudo install -Dm755 inisctl /usr/local/bin/inisctl
sudo install -Dm755 inis-session /usr/local/bin/inis-session
sudo install -Dm644 inis.desktop /usr/share/wayland-sessions/inis.desktop

echo "inis session installed. Select 'inis' in the login manager."
