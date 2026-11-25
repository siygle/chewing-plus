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
set -euo pipefail

if [[ $# -lt 1 ]]; then
    echo "usage: $0 PACKAGE.pkg.tar.zst..." >&2
    exit 2
fi
if [[ $(id -u) -ne 0 ]]; then
    echo "$0 must run as root (pacman -U)" >&2
    exit 1
fi

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
    incoming+=("$(basename "$p" | sed -E 's/-[0-9].*//')")
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
    if pacman -Qq "$name" >/dev/null 2>&1; then
        remove+=("$name")
    fi
done

if [[ ${#remove[@]} -gt 0 ]]; then
    echo "removing conflicting packages: ${remove[*]}"
    pacman -Rdd --noconfirm "${remove[@]}"
fi
pacman -U --noconfirm "${pkgs[@]}"
