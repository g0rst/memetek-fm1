#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Build G0DSGIFT on fm1-dock (https://github.com/joakimeriksson/fm1-dock):
#   build/g0dsgift.dock    the module: drop it on fm1-dock's console page, or tools/dock.py deploy
#   build/g0dsgift.fwsc    a solo firmware: fm1-dock with G0DSGIFT alone, installable like any FM-1 firmware
#
#   ./build.sh [--release X.Y]      needs the JieLi toolchain ($JIELI_TOOLCHAIN) and the AC79 SDK ($AC79_SDK),
#                                   as zvenson/fm1-build installs them on GitHub
#   ./build.sh --fetch-only         only fetch fm1-dock (for tests/run_host.sh)
set -e
cd "$(dirname "$0")"
DOCK_REPO=https://github.com/joakimeriksson/fm1-dock
DOCK_TAG=v0.9.0                 # pinned: the module ABI and the build tools of this release
IDENTITY=FM-1_964               # the solo firmware's package identity
REL=""
FETCH_ONLY=0
while [ $# -gt 0 ]; do
    case "$1" in
    --release) REL="${2#v}"; shift 2 ;;
    --fetch-only) FETCH_ONLY=1; shift ;;
    *) echo "build.sh: unknown option $1"; exit 2 ;;
    esac
done
mkdir -p build
if [ ! -f build/fm1-dock/tools/hbmod.py ]; then
    rm -rf build/fm1-dock
    git clone --depth 1 --branch "$DOCK_TAG" "$DOCK_REPO" build/fm1-dock
fi
[ "$FETCH_ONLY" = 1 ] && exit 0

# the module, inside fm1-dock's tree (its tools find modules/<name>)
rm -rf build/fm1-dock/modules/g0dsgift
cp -r modules/g0dsgift build/fm1-dock/modules/g0dsgift
cd build/fm1-dock
python3 tools/hbmod.py modules/g0dsgift
cp build/modules/g0dsgift.dock ../g0dsgift.dock
# the solo firmware
if [ -n "$REL" ]; then
    python3 tools/build.py --modules modules/g0dsgift --name g0dsgift --app-name "G0DSGIFT" --identity "$IDENTITY" --release "$REL"
    cp "build/g0dsgift-$REL.fwsc" ../ 2>/dev/null || cp build/g0dsgift*.fwsc ../
else
    python3 tools/build.py --modules modules/g0dsgift --name g0dsgift --app-name "G0DSGIFT" --identity "$IDENTITY"
    cp build/g0dsgift*.fwsc ../
fi
cd ../..
ls -l build/*.dock build/*.fwsc
