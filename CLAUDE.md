# Memetek FM1 – notes for working on this repository

Memetek FM1 is the instrument; 8-bit memetek is the genre it is made for. Credits: inspired by g0dsgift777 (keep this in both manuals' thanks section).

- The firmware is an fm1-dock module: `modules/memetek/memetek.c` (+ `memes.inc`). Float DSP, single precision only
  (`-Werror=double-promotion`); `render()` runs in the audio interrupt.
- Test before every push: `tests/run_host.sh` (the module on a fake FM-1 with sanitizers; must print ALL OK).
  For CPU load, run the built `.fwsc` in fm1-emu (`fm1-emu --fwsc ... --keys ... --wav ...`).
- **Both manuals, `docs/HANDBUCH.md` (German) and `docs/MANUAL.md` (English), must be updated with every user-visible
  change**: controls, features, the version table (section 15). Then run `python3 tools/handbuch.py` to regenerate their tables from the source
  (CI fails on `--check` when they are stale). Bump `VERSION` for each release; a push to main publishes
  release `v$(cat VERSION)` and the site (https://g0rst.github.io/memetek-fm1/, including handbuch.html and manual.html).
- The README's control table is a short version of the manual: keep both in step.
- Tags cannot be pushed from the development session: releases come from the workflow.
