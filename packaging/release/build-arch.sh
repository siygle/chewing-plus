#!/usr/bin/env bash
# Build the Arch Linux release packages in a clean archlinux container.
#
#   packaging/release/build-arch.sh OUTDIR
#
# Produces in OUTDIR one all-in-one package per input framework (like the
# macOS pkg), each with libchewing 0.14, the input method, voxkit and data:
#   chewing-plus-fcitx5-*.pkg.tar.zst
#   chewing-plus-ibus-*.pkg.tar.zst
#
# Runs as root (CI container). makepkg runs as an unprivileged "builder".
# CHEWING_PLUS_VERSION (e.g. v6.0.0) sets the package version.
# CHEWING_PLUS_UPGRADE_FROM: a directory of older packages (5.1.13-plus.x);
# when set, the smoke test installs those first and upgrades over them.
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
mkdir -p "$work/bundle-build"
cp "$here/PKGBUILD.bundle" "$work/bundle-build/PKGBUILD"
cp "$here/chewing-plus.install" "$work/bundle-build/"
chown -R builder: "$work"
# libchewing describes its version from git; let builder read root's checkouts.
git config --system --add safe.directory '*'

(cd "$work/bundle-build" && sudo -u builder env \
    CHEWING_PLUS_SRC="$work/chewing" \
    CHEWING_PLUS_LIBCHEWING_SRC="$work/libchewing" \
    CHEWING_PLUS_VOXKIT_SRC="$work/fcitx5-voxkit" \
    CHEWING_PLUS_PKGVER="$pkgver" \
    makepkg --noconfirm --cleanbuild)

mkdir -p "$out"
cp -v "$work"/bundle-build/chewing-plus-*.pkg.tar.zst "$out/"

# Smoke test 1 (optional): upgrade from the separate 5.1.13-plus.x packages.
if [[ -n "${CHEWING_PLUS_UPGRADE_FROM:-}" ]]; then
    pacman -U --noconfirm "$CHEWING_PLUS_UPGRADE_FROM"/*.pkg.tar.zst
    pacman -Q libchewing-plus fcitx5-chewing-plus fcitx5-voxkit
    # replaces= only acts on sysupgrade; a file upgrade answers the conflict
    # prompt (yes = remove the old packages). `yes |` would end in SIGPIPE
    # under pipefail, so the answers come from a here-string.
    pacman -U "$out"/chewing-plus-fcitx5-*.pkg.tar.zst <<<$'y\ny\ny\ny\ny\ny\n'
    if pacman -Q libchewing-plus fcitx5-chewing-plus fcitx5-voxkit 2>/dev/null; then
        echo "old packages are still installed after the upgrade" >&2
        exit 1
    fi
fi

check_common() {
    test -f /usr/share/libchewing/static_dict.bin
    test -x /usr/bin/voxkit
    test -f /usr/lib/systemd/user/voxkit.service
    # Installed, not enabled for anyone.
    ! ls /etc/systemd/user/*/voxkit.service >/dev/null 2>&1
    test ! -e /usr/include/chewing_plus.h
}

# Smoke test 2: each bundle installs on its own and has everything.
pacman -U --noconfirm "$out"/chewing-plus-fcitx5-*.pkg.tar.zst
check_common
test -f /usr/lib/fcitx5/libchewing.so
test -f /usr/lib/fcitx5/voxkit.so
test -f /usr/share/fcitx5/data/chewing-plus-phrases.bin
test -f /usr/share/fcitx5/data/chewing-plus-chars.bin
ldd /usr/lib/fcitx5/libchewing.so | grep -q 'libchewing.so.3 => /usr/lib/libchewing.so.3'
ldd /usr/lib/fcitx5/libchewing.so | grep -q 'not found' && exit 1

# The two bundles carry the same files, so switching replaces one with the
# other.
pacman -U "$out"/chewing-plus-ibus-*.pkg.tar.zst <<<$'y\ny\ny\n'
pacman -Q chewing-plus-fcitx5 2>/dev/null && exit 1
check_common
test -x /usr/libexec/ibus-chewing-plus
test -x /usr/libexec/ibus-setup-chewing-plus
test -f /usr/share/chewing-plus/plus-chars.bin
ldd /usr/libexec/ibus-chewing-plus | grep -q 'not found' && exit 1

echo "packages in $out:"
ls -l "$out"
