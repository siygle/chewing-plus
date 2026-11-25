#!/usr/bin/env bash
# Install one or more chewing-plus-*.pkg.tar.zst files, replacing official
# libchewing / fcitx5-chewing and earlier Plus packages.
#
#   sudo packaging/release/install-arch.sh chewing-plus-fcitx5-*.pkg.tar.zst
#   sudo packaging/release/install-arch.sh chewing-plus-fcitx5-*.pkg.tar.zst \
#       chewing-plus-translate-*.pkg.tar.zst
#
# Why this exists: `replaces=` in the PKGBUILD is honoured on `pacman -Syu`
# (the default answer is Y). A local `pacman -U` still prompts "remove
# conflicting packages?", and `pacman -U --noconfirm` answers N, so the
# upgrade is skipped. This script removes the conflicting packages first
# (`-Rdd`, dependencies are satisfied by the new package) and then
# `pacman -U --noconfirm`.
#
# `pacman -Qq NAME` also matches provides=, so a query for libchewing /
# fcitx5-chewing / fcitx5-voxkit succeeds when chewing-plus-fcitx5 is
# already installed, and a later `pacman -Rdd libchewing` fails with
# "target not found". Only real installed names are removed. Packages
# being installed (chewing-plus-*) are left in place for `pacman -U` to
# upgrade.
set -euo pipefail

if [[ $# -lt 1 ]]; then
    echo "usage: $0 PACKAGE.pkg.tar.zst..." >&2
    exit 2
fi
if [[ $(id -u) -ne 0 ]]; then
    echo "$0 must run as root (pacman -U)" >&2
    exit 1
fi

pkgname_of() {
    local n
    n=$(bsdtar -xOf "$1" .PKGINFO 2>/dev/null | awk '/^pkgname / {print $3; exit}')
    if [[ -z $n ]]; then
        n=$(basename "$1" | sed -E 's/-[0-9].*//')
    fi
    printf '%s\n' "$n"
}

# Real package names only; pacman -Qq NAME also matches provides=.
installed_exact() {
    pacman -Qq 2>/dev/null | grep -qx -- "$1"
}

pkgs=()
for p in "$@"; do
    if [[ ! -f $p ]]; then
        echo "not a file: $p" >&2
        exit 1
    fi
    pkgs+=("$(realpath -m "$p")")
done

# Names we conflict with. Query quietly; missing packages are ignored.
candidates=(
    libchewing
    fcitx5-chewing
    libchewing-plus
    fcitx5-chewing-plus
    ibus-chewing-plus
    fcitx5-voxkit
    chewing-plus-fcitx5
    chewing-plus-ibus
)

# Don't remove a package we are about to install (reinstall / upgrade of itself).
incoming=()
for p in "${pkgs[@]}"; do
    incoming+=("$(pkgname_of "$p")")
done

remove=()
for name in "${candidates[@]}"; do
    skip=
    for inc in "${incoming[@]}"; do
        if [[ $name == "$inc" ]]; then
            skip=1
            break
        fi
    done
    [[ -n $skip ]] && continue
    if installed_exact "$name"; then
        remove+=("$name")
    fi
done

if [[ ${#remove[@]} -gt 0 ]]; then
    echo "removing conflicting packages: ${remove[*]}"
    pacman -Rdd --noconfirm "${remove[@]}"
else
    echo "no conflicting packages to remove"
fi
pacman -U --noconfirm "${pkgs[@]}"
