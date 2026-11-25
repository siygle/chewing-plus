#!/usr/bin/env bash
# Build the pinned libchewing and install it into a prefix.
#
#   scripts/bundle-libchewing.sh [--shared|--static] [PREFIX]
#   scripts/bundle-libchewing.sh --fetch-only
#
# --fetch-only clones and verifies the checkout (with dictionary data) and
#   stops before building. Release packaging builds it with makepkg.
#
# --shared (default) installs a normal libchewing.so. Point pkg-config at
#   PREFIX/lib/pkgconfig, or pass the prefix to CMake:
#     -DCHEWING_PLUS_BUNDLE_LIBCHEWING=ON
#     -DCHEWING_PLUS_LIBCHEWING_PREFIX=PREFIX
# --static asks libchewing for libchewing.a and copies libchewing_capi.a
#   (the Rust staticlib) beside it. v0.14.0-rc.2's own CMake refuses to
#   generate that build (chewing_capi is missing from the export set); use
#   --shared until upstream fixes it. -DCHEWING_PLUS_LIBCHEWING_STATIC=ON
#   consumes a prefix that did install the archives.
#
# The pin is cmake/ChewingPlusLibchewing.cmake (v0.14.0-rc.2,
# 1bfcde9078f13b4d97aaa4b6a01cbd67687522e3).
# Override the checkout with CHEWING_PLUS_LIBCHEWING_SRC.
set -euo pipefail

VERSION=0.14.0-rc.2
URL=https://github.com/siygle/libchewing.git
COMMIT=1bfcde9078f13b4d97aaa4b6a01cbd67687522e3

mode=shared
prefix=
while [[ $# -gt 0 ]]; do
    case "$1" in
        --static) mode=static; shift ;;
        --shared) mode=shared; shift ;;
        --fetch-only) mode=fetch; shift ;;
        -h|--help)
            sed -n '2,20p' "$0"
            exit 0
            ;;
        *)
            prefix=$1
            shift
            ;;
    esac
done

root=$(cd "$(dirname "$0")/.." && pwd)
prefix=${prefix:-$root/third_party/dist}
src=${CHEWING_PLUS_LIBCHEWING_SRC:-$root/third_party/libchewing}

if [[ ! -d "$src/.git" ]]; then
    # Keep tags in the main repo so libchewing's `git describe` emits a
    # valid semver-prefixed version for chewing.pc (the heavy data submodule
    # is still fetched shallow below).
    git clone --no-checkout "$URL" "$src"
    git -C "$src" checkout --detach "$COMMIT"
fi
# Dictionary sources live in the libchewing-data submodule, and the large
# language-model files are Git LFS pointers until `git lfs pull`.
if ! git lfs version >/dev/null 2>&1; then
    echo "git-lfs is required to fetch libchewing dictionary data" >&2
    exit 1
fi
# The filter has to be installed before the submodule checkout. Otherwise
# `git submodule update` leaves the language-model files as LFS pointers
# ("Skipping object checkout, Git LFS is not installed") and a later
# `git lfs pull` can no-op when GIT_LFS_SKIP_SMUDGE is set.
git lfs install --skip-repo
git -C "$src" submodule update --init --depth 1 --recursive
git -C "$src/data" lfs install --local
env -u GIT_LFS_SKIP_SMUDGE git -C "$src/data" lfs pull
arpa="$src/data/dict/chewing_v4/static_lm.arpa"
if [[ ! -f "$arpa" ]] || head -n 1 "$arpa" | grep -q 'git-lfs.github.com'; then
    echo "libchewing dictionary data is still a Git LFS pointer: $arpa" >&2
    exit 1
fi
got=$(git -C "$src" rev-parse HEAD)
if [[ "$got" != "$COMMIT" ]]; then
    echo "libchewing checkout $src is $got, expected $COMMIT ($VERSION)" >&2
    echo "Delete it or set CHEWING_PLUS_LIBCHEWING_SRC to a checkout of that commit." >&2
    exit 1
fi
if [[ "$mode" == fetch ]]; then
    echo "libchewing ${VERSION} (${COMMIT}) checked out at ${src}"
    exit 0
fi

if command -v ninja >/dev/null 2>&1; then
    gen=Ninja
else
    gen="Unix Makefiles"
fi
shared=ON
if [[ "$mode" == static ]]; then
    shared=OFF
fi

build="$src/build-$mode"
# Configure from the libchewing checkout. Its CMakeLists runs
# `git --work-tree <src> describe` without --git-dir, which describes
# whatever repository the current directory belongs to.
(
    cd "$src"
    cmake -S "$src" -B "$build" -G "$gen" \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX="$prefix" \
        -DBUILD_SHARED_LIBS="$shared" \
        -DBUILD_TESTING=OFF \
        -DBUILD_DOC=OFF
    cmake --build "$build"
    cmake --install "$build"
)

if [[ "$mode" == static ]]; then
    libdir="$prefix/lib"
    if [[ -d "$prefix/lib64" && ! -d "$prefix/lib" ]]; then
        libdir="$prefix/lib64"
    fi
    mkdir -p "$libdir"
    found=0
    while IFS= read -r -d '' archive; do
        cp -v "$archive" "$libdir/"
        found=1
    done < <(find "$build" -name 'libchewing_capi*.a' -print0)
    if [[ "$found" -ne 1 ]]; then
        echo "warning: libchewing_capi staticlib was not found; static link may be incomplete" >&2
    fi
fi

echo "libchewing ${VERSION} (${COMMIT}) installed to ${prefix} (${mode})"
