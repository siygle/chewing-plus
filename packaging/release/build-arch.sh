#!/usr/bin/env bash
# Build the Arch Linux release packages in a clean archlinux container.
#
#   packaging/release/build-arch.sh OUTDIR
#
# Produces in OUTDIR:
#   libchewing-plus-*.pkg.tar.zst      libchewing 0.14 (pinned tag)
#   fcitx5-voxkit-*.pkg.tar.zst        dictation daemon + fcitx5 module
#   fcitx5-chewing-plus-*.pkg.tar.zst  fcitx5 addon
#   ibus-chewing-plus-*.pkg.tar.zst    IBus engine + setup window
#
# Runs as root (CI container). makepkg runs as an unprivileged "builder".
# CHEWING_PLUS_VERSION (e.g. 5.1.13-plus.1) sets the package version.
set -euo pipefail

out=$(realpath -m "${1:?usage: build-arch.sh OUTDIR}")
root=$(cd "$(dirname "$0")/../.." && pwd)
here="$root/packaging/release"
version=${CHEWING_PLUS_VERSION:-$(git -C "$root" describe --tags --always)}
# pacman versions cannot contain '-'.
pkgver=$(printf '%s' "$version" | sed 's/^v//; s/-/./g')

# Same fcitx5-voxkit commit as macos/build-voxkit.sh.
VOXKIT_URL=https://github.com/siygle/fcitx5-voxkit.git
VOXKIT_COMMIT=$(cat "$here/voxkit-pin")

if [[ $(id -u) -ne 0 ]]; then
    echo "build-arch.sh expects to run as root in a throwaway container" >&2
    exit 1
fi

pacman -Syu --noconfirm --needed base-devel git git-lfs cmake ninja rust \
    extra-cmake-modules fcitx5 json-c alsa-lib openblas opencc ibus libadwaita

work=$(mktemp -d /tmp/chewing-plus-release.XXXXXX)
id builder >/dev/null 2>&1 || useradd -m builder
# Pull every source as root first; makepkg itself needs no network.
CHEWING_PLUS_LIBCHEWING_SRC="$work/libchewing" \
    bash "$root/scripts/bundle-libchewing.sh" --fetch-only
git clone -q "$VOXKIT_URL" "$work/fcitx5-voxkit"
git -C "$work/fcitx5-voxkit" checkout -q --detach "$VOXKIT_COMMIT"
git -C "$root" archive --format=tar HEAD | (mkdir -p "$work/chewing" && tar -x -C "$work/chewing")
for d in libchewing voxkit chewing-pkg; do
    mkdir -p "$work/$d-build"
done
cp "$here/PKGBUILD.libchewing" "$work/libchewing-build/PKGBUILD"
cp "$here/PKGBUILD.voxkit" "$work/voxkit-build/PKGBUILD"
cp "$here/PKGBUILD.chewing" "$work/chewing-pkg-build/PKGBUILD"
chown -R builder: "$work"
# libchewing describes its version from git; let builder read root's checkouts.
git config --system --add safe.directory '*'

vk_count=$(git -C "$work/fcitx5-voxkit" rev-list --count HEAD)
vk_short=$(git -C "$work/fcitx5-voxkit" rev-parse --short HEAD)

build() {
    local dir=$1
    shift
    (cd "$work/$dir" && sudo -u builder env "$@" makepkg --noconfirm --cleanbuild)
}

build libchewing-build CHEWING_PLUS_LIBCHEWING_SRC="$work/libchewing" \
    CHEWING_PLUS_LC_PKGVER=0.14.0rc1
pacman -U --noconfirm "$work"/libchewing-build/libchewing-plus-*.pkg.tar.zst

build voxkit-build CHEWING_PLUS_VOXKIT_SRC="$work/fcitx5-voxkit" \
    CHEWING_PLUS_VK_PKGVER="0.1.0.r${vk_count}.g${vk_short}"
pacman -U --noconfirm "$work"/voxkit-build/fcitx5-voxkit-*.pkg.tar.zst

build chewing-pkg-build CHEWING_PLUS_SRC="$work/chewing" CHEWING_PLUS_PKGVER="$pkgver"

mkdir -p "$out"
cp -v "$work"/*-build/*.pkg.tar.zst "$out/"
# Smoke test: the packages install together on a clean system.
pacman -U --noconfirm "$out"/fcitx5-chewing-plus-*.pkg.tar.zst "$out"/ibus-chewing-plus-*.pkg.tar.zst
test -f /usr/lib/fcitx5/libchewing.so
test -x /usr/libexec/ibus-chewing-plus
test -x /usr/libexec/ibus-setup-chewing-plus
test -x /usr/bin/voxkit
test -f /usr/share/libchewing/static_dict.bin
ldd /usr/lib/fcitx5/libchewing.so | grep -q 'libchewing.so.3'
echo "packages in $out:"
ls -l "$out"
