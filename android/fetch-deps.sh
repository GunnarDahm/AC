#!/bin/sh
# Downloads the third-party sources the Android build compiles from (pinned versions).
# Run once before building: ./fetch-deps.sh
set -e
cd "$(dirname "$0")"
mkdir -p deps

# applies android/patches/<name>/*.patch on top of a pristine checkout
patch_dep() # name
{
    git -C "deps/$1" checkout -q -- .
    for p in patches/"$1"/*.patch; do
        [ -f "$p" ] || continue
        git -C "deps/$1" apply "$(pwd)/$p"
        echo "deps/$1: applied $(basename "$p")"
    done
}

fetch() # name url ref commit
{
    if [ -d "deps/$1/.git" ] && [ "$(git -C "deps/$1" rev-parse HEAD)" = "$4" ]; then
        echo "deps/$1 is up to date"
        patch_dep "$1"
        return
    fi
    rm -rf "deps/$1"
    git init -q "deps/$1"
    git -C "deps/$1" fetch -q --depth 1 "$2" "$3"
    git -C "deps/$1" checkout -q FETCH_HEAD
    actual="$(git -C "deps/$1" rev-parse HEAD)"
    if [ "$actual" != "$4" ]; then
        echo "deps/$1: expected commit $4 but got $actual" >&2
        exit 1
    fi
    echo "fetched deps/$1 ($3)"
    patch_dep "$1"
}

fetch SDL2        https://github.com/libsdl-org/SDL.git       release-2.30.9 c98c4fbff6d8f3016a3ce6685bf8f43433c3efcc
fetch SDL2_image  https://github.com/libsdl-org/SDL_image.git release-2.8.2  abcf63aa71b4e3ac32120fa9870a6500ddcdcc89
fetch gl4es       https://github.com/ptitSeb/gl4es.git        c1d7d014fcf687ed33f30ce78b6480b3be1cbeee c1d7d014fcf687ed33f30ce78b6480b3be1cbeee
fetch openal-soft https://github.com/kcat/openal-soft.git     1.23.1         d3875f333fb6abe2f39d82caca329414871ae53b
fetch ogg         https://github.com/xiph/ogg.git             v1.3.5         e1774cd77f471443541596e09078e78fdc342e4f
fetch vorbis      https://github.com/xiph/vorbis.git          v1.3.7         0657aee69dec8508a0011f47f3b69d7538e9d262
