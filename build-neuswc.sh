#!/bin/sh
set -eu

prefix="${NEU_PREFIX:-$HOME/.local/inis-neu-prefix}"
src_root="${NEU_SRC_ROOT:-/tmp}"
jobs="${JOBS:-}"

neuwld_dir="$src_root/neuwld"
neuswc_dir="$src_root/neuswc"

run_compile() {
	dir=$1
	if [ -n "$jobs" ]; then
		meson compile -C "$dir/build" -j "$jobs"
	else
		meson compile -C "$dir/build"
	fi
}

clone_or_update() {
	url=$1
	dir=$2

	if [ -d "$dir/.git" ]; then
		if [ "${NO_UPDATE:-0}" = "1" ]; then
			echo "using existing source: $dir"
		else
			git -C "$dir" pull --ff-only
		fi
	else
		if [ "${NO_UPDATE:-0}" = "1" ]; then
			echo "missing source and NO_UPDATE=1: $dir" >&2
			exit 1
		fi
		git clone "$url" "$dir"
	fi
}

clone_or_update "https://git.sr.ht/~shrub900/neuwld" "$neuwld_dir"
clone_or_update "https://git.sr.ht/~shrub900/neuswc" "$neuswc_dir"

if [ ! -d "$neuwld_dir/build" ]; then
	meson setup "$neuwld_dir/build" "$neuwld_dir" --prefix="$prefix"
fi
run_compile "$neuwld_dir"
meson install -C "$neuwld_dir/build"

export PKG_CONFIG_PATH="$prefix/lib64/pkgconfig:$prefix/lib/pkgconfig:${PKG_CONFIG_PATH:-}"

if [ ! -d "$neuswc_dir/build" ]; then
	meson setup "$neuswc_dir/build" "$neuswc_dir" --prefix="$prefix"
fi
run_compile "$neuswc_dir"
meson install -C "$neuswc_dir/build"

echo "neuswc stack installed to: $prefix"
echo "Use:"
echo "  PKG_CONFIG_PATH=$prefix/lib64/pkgconfig:$prefix/lib/pkgconfig NEUSWC_PREFIX=$prefix make USE_NEUSWC=1"
echo "  ./inis --features  # rpath embeds library path, no LD_LIBRARY_PATH needed"
echo "  make USE_NEUSWC=1 install  # instala in /usr/local"
