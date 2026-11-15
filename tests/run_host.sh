#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Memetek FM1 on this computer: build/host/memetek.wav, the screens as PNG, and the checks (tests/host.c).
#   FM1_DOCK  an fm1-dock checkout (default: build/fm1-dock, as ./build.sh fetches it)
set -e
cd "$(dirname "$0")/.."
D="${FM1_DOCK:-build/fm1-dock}"
[ -f "$D/firmware/dock/hb_module.h" ] || ./build.sh --fetch-only
mkdir -p build/host
${CC:-cc} -std=gnu11 -O1 -g -Wall -Wextra -Wno-unused-function -Wno-unused-parameter -Werror=double-promotion \
    ${SAN:--fsanitize=address,undefined} \
    -I"$D/firmware/dock" -I"$D/firmware/src" -I"$D/firmware/gen" -o build/host/memetek tests/host.c -lm
build/host/memetek build/host
python3 - <<'PY' || true
from pathlib import Path
from PIL import Image
for p in sorted(Path("build/host").glob("memetek-*.ppm")):
    Image.open(p).resize((480, 480), Image.NEAREST).save(p.with_suffix(".png"))
    p.unlink()
    print("screen ->", p.with_suffix(".png"))
PY
