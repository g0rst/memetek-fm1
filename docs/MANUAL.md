# Memetek FM1 – Manual

<!-- GEN:VERSION -->
**Version 0.6**
<!-- /GEN:VERSION -->
 · the groovebox for **8-bit memetek** on the M-VAVE FM-1 · [Deutsches Handbuch](handbuch.html) · [Site with emulator](https://g0rst.github.io/memetek-fm1/) · [Source](https://github.com/g0rst/memetek-fm1)

> This manual belongs to the firmware and is updated with every new version. The parameter and sound tables are generated
> straight from the firmware's source.
> Memetek FM1 is young and has so far only been tested in the FM-1 emulator. Installing it on real hardware is at your own risk.

[TOC]

## 1. What is Memetek FM1?

**8-bit memetek** is the genre: tek kicks and rolling bass, the 8-bit sound of old game consoles and meme sounds, loud,
fast and silly. **memetek-fm1** is the instrument for it. It turns the FM-1 into a groovebox for exactly this music:

- **6 tracks:** TEK-KICK, HI-HATS, 8BIT BASS, CHIP ARP, 8BIT LEAD, MEME FX
- **16-step sequencer** with 4 patterns, chain, copy, shift, dice and live recording
- **96 meme sounds** in 6 banks, every one synthesised live (no samples)
- **Synthesis:** square, pulse, saw, triangle, **supersaw**, FM, PWM, sync and fold for bass, arp and lead
- **Sound pages:** the 4 knobs always control the 4 parameters of the current page, plus 5 LFOs (2× bass, arp, lead, meme)
- **Mixer** with volume, pan and echo per track, and a bitcrusher, drive and compressor on the master

## 2. Installing

### Try it without a device

On the [Memetek FM1 site](https://g0rst.github.io/memetek-fm1/), tap **"Im FM-1-Emulator testen"** (try in the FM-1 emulator).
The very same firmware runs there in an emulated FM-1. On old computers the emulator may be too slow; a current phone runs
it well.

### Onto the FM-1

You need Chrome or Edge on a computer and a USB data cable.

1. **Install fm1-dock** (once): on the [fm1-dock page](https://joakimeriksson.github.io/fm1-dock/#install) choose "Connect over
   USB", then "Install fm1-dock".
2. **Load Memetek FM1:** download `memetek-fm1.dock` from the [latest release](https://github.com/g0rst/memetek-fm1/releases) and drop it on
   the [fm1-dock console](https://joakimeriksson.github.io/fm1-dock/#console).
3. On the FM-1, hold **HOME** for about 1.5 seconds and pick **MEMETEK FM1**.

`memetek-fm1.fwsc` is the same firmware as a standalone package (fm1-dock with Memetek FM1 alone).

**Updates:** drop the new `memetek-fm1.dock` on the console again. If your module is still called G0DSGIFT or MEMETEK, delete the old one in the fm1-dock
console.

**Back to the stock firmware:** with the [FM-1 Firmware Hub](https://fm1.designburgapps.com) or M-VAVE's own updater. A module
that crashes twice in a row is skipped by fm1-dock at start-up.

## 3. The panel

```
  MASTER   SELECT     ┌──────────┐    KNOB1   KNOB2   KNOB3   KNOB4
                      │ display  │
  PRESETS  ALGORITHM  │          │    [FX]  [SEL] [ENV] [LFO] [EDIT] [GLO]
                      └──────────┘    [HOME][SAVE][ARP] [SEQ] [PLAY] [REC]
  [OCT-] [OCT+]

  upper row:  OP1 OP2 OP3   OP4 OP5   OP6 PIT GLO   MONO POLY   (right)
  lower row:  16 keys = 16 steps
```

| Control | Function |
|---|---|
| **MASTER** | overall volume |
| **SELECT** | pick the track (KICK → HATS → BASS → ARP → LEAD → MEME) |
| **ALGORITHM** | page of the current track |
| **PRESETS** | pick a meme sound (all 96); stopped, you hear it at once |
| **KNOB 1–4** | the page's 4 parameters, shown as K1–K4 on the display |
| **PLAY/STOP** | start or stop the sequencer |
| **REC** | record on/off (see [Recording](#7-recording)) |
| **SEQ** | switch between STEP mode and PLAY mode |
| **ENV** | to the current track's pages |
| **LFO** | straight to the track's LFO page (bass, arp, lead, meme) |
| **FX** | mixer |
| **GLO** | global: tempo, swing, crush, drive |
| **EDIT** | copy the pattern into the next one and go there |
| **ARP** | dice: a random, fitting pattern for the current track |
| **SEL + ARP** | clear the current track in the pattern |
| **SEL** tapped | tap tempo: tap a few times in time |
| **SAVE** | keep everything in flash (only when stopped) |
| **HOME** tapped | back: the track's first page, STEP mode |
| **HOME** held (1.5 s) | the fm1-dock picker (other synths) |
| **OCT− / OCT+** | STEP mode: shift the track by one step · PLAY mode: octave of the keys |

## 4. The display

- **Header:** on the left MEMETEK FM1 and the pattern being **edited** (`P2*` = edits not yet activated), on the right the
  **playing** pattern with `>` (running) or `||` (stopped), `C` for chain, and the tempo, e.g. `>P1 160`
- **Title:** the track in its colour (or MIXER / GLOBAL), on the right the page, e.g. `S2/5`
- **Step grid:** 16 cells in two rows. Coloured: a step that is set; white: the step playing. The label in a cell shows its
  value (see below). On LEAD a bar marks the recording cursor
- **Parameters:** the page's 4 parameters with value and bar (K1–K4 = KNOB 1–4)
- **Footer:** the tracks `K H B A L M` (grey = muted, yellow = solo, highlighted = selected), the mode, the current meme
  sound with its bank (e.g. `F:APPLAUS`) and the live pitch. Red while recording. Short messages show here in yellow

## 5. STEP mode and PLAY mode

**SEQ** switches between the two. The key LEDs show it: in STEP mode the set steps and the running step light up.

### STEP mode: programming

- **The 16 lower keys** set the steps of the current track. Tapping again steps through the values:

| Track | Values when tapping | Shown as |
|---|---|---|
| KICK | off → on → accent | empty, coloured, `!` |
| HATS | off → closed → open → roll | `x`, `O`, `R` |
| BASS | off → root → fifth → octave | `1`, `5`, `8` |
| ARP | off → chord on degree I → IV → V | `I`, `IV`, `V` |
| LEAD | an empty step: sets a note (the key's root) · a set step at the cursor: clears it | note name |
| MEME | sets the selected meme sound · if it is already there, clears it | 3-letter code |

- **Upper row:**

| Key | Function |
|---|---|
| **OP1–OP6** | mute KICK, HATS, BASS, ARP, LEAD, MEME |
| **SEL + OP1–OP6** | solo (several at once). With a solo, only the solo tracks play |
| **PIT, GLO, MONO, POLY** | pattern 1–4. **First press:** show and edit that pattern, the current one keeps playing. **Second press:** activate it, it plays from the end of the bar (see [Patterns](#6-patterns-chain-copy-dice)) |
| **the key on the right** | chain on/off |

- **OCT− / OCT+** shift the track one step left or right.

### PLAY mode: playing

The keys play the current track:

| Track | lower row | upper row |
|---|---|---|
| KICK | kick | kick with accent |
| HATS | closed | open |
| BASS, LEAD | chromatic (F3–G5, moved with OCT) | chromatic |
| ARP | arpeggio | arpeggio |
| MEME | the 16 sounds of the current bank | **OP1–OP6:** bank A–F · **PIT, GLO, MONO, POLY, right:** pitch −12, −5, 0, +7, +12 |

The memes' live pitch applies to the memes in the running pattern too: whole breaks transpose.

## 6. Patterns, chain, copy, dice

- **4 patterns** (P1–P4), each with all 6 tracks. In STEP mode with **PIT, GLO, MONO, POLY**:
  - **First press** of a pattern key: the display jumps to that pattern and every edit goes there only. The pattern that was
    playing keeps playing unchanged, so your edits aren't heard yet. The header shows `P2*` while there are edits not yet
    activated; the edited pattern's LED blinks
  - **Second press** of the same key: the pattern is **activated**. The edits are taken over and it plays from the end of the
    bar. The playing pattern's LED is lit
  - Editing the pattern that is active, you hear every change at once
- **Chain:** with the key on the right of the upper row, Memetek FM1 plays the next pattern that isn't empty after each bar.
  In a chain, an edited pattern keeps playing its old version until you activate it with its key (one press while it is shown)
- **EDIT** copies the current pattern into the next one (P1 → P2 …) and shows it. One press of its key activates it
- **ARP** rolls the dice for the current track: kicks on the quarters, open hats on the offbeats, a rolling bass between the
  kicks, memes from the current bank
- **SEL + ARP** clears the current track
- **OCT− / OCT+** in STEP mode shift the track

**Factory patterns:** P1 tek · P2 memecore · P3 chiptune with a melody · P4 meme break

## 7. Recording

1. Pick the track and switch to **PLAY mode** with **SEQ**
2. Press **REC**; the footer turns red
3. **While running:** what you play is written to the current step
4. **Stopped:** each key writes to the cursor, which then moves on a step (step entry). For LEAD, set the cursor in STEP mode
   (with REC on) by tapping a step
5. **REC** again ends the recording

What is recorded: KICK (upper row = accent), HATS (upper row = open), BASS and ARP (root), LEAD (the note played), MEME
(the sound played).

## 8. Saving

**SAVE** (only when stopped) keeps all 4 patterns, every parameter, the mixer and the chain in the FM-1's flash. The next
time Memetek FM1 starts, it is all back. The sound drops out briefly while saving; that is normal.

## 9. The tracks and their parameters

Pick the track with **SELECT**, the page with **ALGORITHM**. **KNOB 1–4** control the page's 4 parameters. **LFO** jumps to
the LFO page.

- **TEK-KICK:** a pitched, distorted kick. TUNE is the final pitch, DROP and DROP-T the pitch drop, DECAY the tail. DIST picks
  the distortion: SOFT, HARD or FOLD (wavefolder, metallic)
- **HI-HATS:** 5 kinds (METAL, NOISE, 8-BIT, SHAKER, RIDE). ROLL sets how many hits a roll step has. HUMANIZE varies the
  level, CHANCE the probability of each hit. ACCENT stresses the offbeats, STEREO spreads the hats, CHOKE lets closed hats cut
  open ones
- **8BIT BASS:** an oscillator (see [Synthesis](#synthesis)) with a sub and a resonant filter (LOWPASS, BANDPASS, HIGHPASS on
  page 4). F-ENV is the filter envelope, DECAY the note length in steps. **ROOT is the key for bass, arp and dice melodies.**
  DUCK ducks the bass on each kick (pumping). Pages 5 and 6 are two LFOs
- **CHIP ARP:** a Game Boy arpeggio per step. RATE is the notes per step, RICHTG (direction) up, down, ping-pong or random,
  AKKORD (chord) minor, major, sus or power. Page 3 is the LFO, page 4 the synthesis
- **8BIT LEAD:** a monophonic melody voice with square, pulse, triangle, saw and a NOISE channel like old consoles, plus all
  of the bass's synthesis. LAENGE (length) in steps, GLIDE, SWEEP (the pitch slides in on each note), DETUNE, envelope and a
  chip arpeggio. Page 4 is the LFO, page 5 the synthesis
- **MEME FX:** PITCH transposes all memes, LAENGE (length) shortens and speeds them up. MODE STUTTER plays four short hits per
  half step. RND-PITCH detunes each hit at random. ECHO, E-ZEIT (echo time) and FEEDBK set the echo. Page 3 is the meme LFO
  with the targets PITCH (every meme sounding), VOL, TONE (filter), PAN, ECHO (send) and LAENGE (length, per hit)

### Synthesis

Bass, arp and lead share one oscillator. **WAVE** picks the kind of synthesis, **TIMBRE** shapes it:

| WAVE | Sound | TIMBRE sets |
|---|---|---|
| SQUARE, PULSE, SAW, TRI | the classic 8-bit waves, raw and unfiltered on purpose | (nothing) |
| **SUPERSAW** | seven detuned saws: wide and fat, trance and hardstyle | the detune |
| **FM** | two-operator FM, the modulator an octave up: bells, growl, metal | the modulation index |
| **PWM** | a square with a moving pulse width: floating, chorus-like | the depth of the movement |
| **SYNC** | a hard-synced saw: cutting, sweeps sound like lasers | the slave's pitch |
| **FOLD** | a sine through a wavefolder: from round to screaming | the drive |
| PULSE12, NOISE (lead only) | a thin pulse, the noise channel of old consoles | (nothing) |

- **T-ENV:** an envelope on TIMBRE for each note. Positive starts bright and closes, negative the other way: an FM bass
  "wows", a supersaw opens up
- **WIDTH:** the supersaw's stereo width
- **FILTER** (bass only): LOWPASS, BANDPASS or HIGHPASS
- The LFOs can target **TIMBRE**: drifting supersaws, FM wobble, moving folds

### LFOs

Bass (2×), arp, lead and meme have LFOs with **ZIEL** (target: what they modulate, TIMBRE included), **RATE** (in time: 4 bars to
1/32), **DEPTH** (both ways, negative = inverted) and **FORM** (shape: sine, triangle, saw, square, S&H = random per period).
The LFOs run in time with the tempo and restart with PLAY.

### All parameters

<!-- GEN:PARAMS -->
#### TEK-KICK

| Page | Knob | Parameter | Range / values | Default |
|---|---|---|---|---|
| 1 | K1 | TUNE | 30 Hz … 80 Hz | 42 Hz |
| 1 | K2 | DECAY | 0.15 s … 0.9 s | 0.45 s |
| 1 | K3 | DROP | x2 … x10 | x5 |
| 1 | K4 | DROP-T | 10 ms … 150 ms | 60 ms |
| 2 | K1 | DRIVE | 0 % … 100 % | 30 % |
| 2 | K2 | DIST | SOFT · HARD · FOLD | SOFT |
| 2 | K3 | CLICK | 0 % … 100 % | 60 % |
| 2 | K4 | VOL | 0 % … 100 % | 90 % |

#### HI-HATS

| Page | Knob | Parameter | Range / values | Default |
|---|---|---|---|---|
| 1 | K1 | TUNE | x0.5 … x2 | x1 |
| 1 | K2 | ZU | 10 ms … 100 ms | 35 ms |
| 1 | K3 | OFFEN | 80 ms … 600 ms | 220 ms |
| 1 | K4 | VOL | 0 % … 100 % | 30 % |
| 2 | K1 | TYP | METAL · NOISE · 8-BIT · SHAKER · RIDE | METAL |
| 2 | K2 | ROLL | 2x · 3x · 4x · 6x | 3x |
| 2 | K3 | HUMANIZE | 0 % … 100 % | 0 % |
| 2 | K4 | CHANCE | 0 % … 100 % | 100 % |
| 3 | K1 | TONE | x0.3 … x1.5 | x1 |
| 3 | K2 | ACCENT | 0 % … 100 % | 0 % |
| 3 | K3 | STEREO | 0 % … 100 % | 0 % |
| 3 | K4 | CHOKE | AUS · AN | AN |

#### 8BIT BASS

| Page | Knob | Parameter | Range / values | Default |
|---|---|---|---|---|
| 1 | K1 | CUTOFF | 100 Hz … 5000 Hz | 2400 Hz |
| 1 | K2 | RESO | 0.5 … 20 | 6 |
| 1 | K3 | F-ENV | 0 % … 100 % | 80 % |
| 1 | K4 | DECAY | x0.2 … x1.5 | x0.9 |
| 2 | K1 | WAVE | SQUARE · PULSE · SAW · TRI · SUPERSAW · FM · PWM · SYNC · FOLD | SQUARE |
| 2 | K2 | TIMBRE | 0 % … 100 % | 50 % |
| 2 | K3 | SUB | 0 % … 100 % | 50 % |
| 2 | K4 | VOL | 0 % … 100 % | 50 % |
| 3 | K1 | ROOT | A · A# · B · C · C# · D · D# · E · F · F# · G · G# | D |
| 3 | K2 | OCT | -1 … +1 | 0 |
| 3 | K3 | DUCK | 0 % … 100 % | 0 % |
| 3 | K4 | DRIVE | 0 % … 100 % | 0 % |
| 4 | K1 | GLIDE | 0 ms … 200 ms | 0 ms |
| 4 | K2 | T-ENV | -100 % … +100 % | 0 % |
| 4 | K3 | WIDTH | 0 % … 100 % | 50 % |
| 4 | K4 | FILTER | LOWPASS · BANDPASS · HIGHPASS | LOWPASS |
| 5 | K1 | LFO1 ZIEL | CUTOFF · RESO · PITCH · F-ENV · DECAY · SUB · VOL · TIMBRE | CUTOFF |
| 5 | K2 | RATE | 4BAR · 2BAR · 1BAR · 1/2 · 1/4 · 1/8 · 1/16 · 1/32 | 1/4 |
| 5 | K3 | DEPTH | -100 % … +100 % | 0 % |
| 5 | K4 | FORM | SINE · TRI · SAW · SQUARE · S&H | SINE |
| 6 | K1 | LFO2 ZIEL | CUTOFF · RESO · PITCH · F-ENV · DECAY · SUB · VOL · TIMBRE | PITCH |
| 6 | K2 | RATE | 4BAR · 2BAR · 1BAR · 1/2 · 1/4 · 1/8 · 1/16 · 1/32 | 1/16 |
| 6 | K3 | DEPTH | -100 % … +100 % | 0 % |
| 6 | K4 | FORM | SINE · TRI · SAW · SQUARE · S&H | SINE |

#### CHIP ARP

| Page | Knob | Parameter | Range / values | Default |
|---|---|---|---|---|
| 1 | K1 | RATE | 2/STP · 3/STP · 4/STP · 6/STP | 4/STP |
| 1 | K2 | RICHTG | UP · DOWN · PING · RND | UP |
| 1 | K3 | WAVE | SQUARE · PULSE · SAW · TRI · SUPERSAW · FM · PWM · SYNC · FOLD | SQUARE |
| 1 | K4 | VOL | 0 % … 60 % | 22 % |
| 2 | K1 | OCT | -1 … +2 | 0 |
| 2 | K2 | DECAY | 20 % … 100 % | 98 % |
| 2 | K3 | VIBRATO | 0 % … 100 % | 0 % |
| 2 | K4 | AKKORD | MOLL · DUR · SUS · POWER | MOLL |
| 3 | K1 | LFO ZIEL | PITCH · VOL · TONE · PAN · DECAY · OKTAVE · TIMBRE | PITCH |
| 3 | K2 | RATE | 4BAR · 2BAR · 1BAR · 1/2 · 1/4 · 1/8 · 1/16 · 1/32 | 1/8 |
| 3 | K3 | DEPTH | -100 % … +100 % | 0 % |
| 3 | K4 | FORM | SINE · TRI · SAW · SQUARE · S&H | SINE |
| 4 | K1 | TIMBRE | 0 % … 100 % | 50 % |
| 4 | K2 | T-ENV | -100 % … +100 % | 0 % |
| 4 | K3 | WIDTH | 0 % … 100 % | 50 % |

#### 8BIT LEAD

| Page | Knob | Parameter | Range / values | Default |
|---|---|---|---|---|
| 1 | K1 | WAVE | SQUARE · PULSE25 · PULSE12 · TRI · SAW · NOISE · SUPERSAW · FM · PWM · SYNC · FOLD | SQUARE |
| 1 | K2 | LAENGE | x0.25 … x4 | x1 |
| 1 | K3 | VIBRATO | 0 % … 100 % | 0 % |
| 1 | K4 | VOL | 0 % … 100 % | 35 % |
| 2 | K1 | OCT | -2 … +2 | 0 |
| 2 | K2 | GLIDE | 0 ms … 300 ms | 0 ms |
| 2 | K3 | SWEEP | -12 … +12 | 0 |
| 2 | K4 | DETUNE | 0 % … 100 % | 0 % |
| 3 | K1 | ATTACK | 0 ms … 300 ms | 0 ms |
| 3 | K2 | DECAY | 0.05 s … 2 s | 0.4 s |
| 3 | K3 | SUSTAIN | 0 % … 100 % | 60 % |
| 3 | K4 | ARP | AUS · OKTAVE · DUR · MOLL | AUS |
| 4 | K1 | LFO ZIEL | PITCH · VOL · TONE · PAN · TIMBRE | PITCH |
| 4 | K2 | RATE | 4BAR · 2BAR · 1BAR · 1/2 · 1/4 · 1/8 · 1/16 · 1/32 | 1/4 |
| 4 | K3 | DEPTH | -100 % … +100 % | 0 % |
| 4 | K4 | FORM | SINE · TRI · SAW · SQUARE · S&H | SINE |
| 5 | K1 | TIMBRE | 0 % … 100 % | 50 % |
| 5 | K2 | T-ENV | -100 % … +100 % | 0 % |
| 5 | K3 | WIDTH | 0 % … 100 % | 50 % |

#### MEME FX

| Page | Knob | Parameter | Range / values | Default |
|---|---|---|---|---|
| 1 | K1 | PITCH | -12 … +12 | 0 |
| 1 | K2 | LAENGE | 10 % … 100 % | 100 % |
| 1 | K3 | MODE | NORMAL · STUTTER | NORMAL |
| 1 | K4 | VOL | 0 % … 100 % | 50 % |
| 2 | K1 | RND-PITCH | 0 … 12 | 0 |
| 2 | K2 | ECHO | 0 % … 100 % | 0 % |
| 2 | K3 | E-ZEIT | 1/16 · 1/8 · 3/16 · 1/4 | 1/8 |
| 2 | K4 | FEEDBK | 0 % … 90 % | 40 % |
| 3 | K1 | LFO ZIEL | PITCH · VOL · TONE · PAN · ECHO · LAENGE | PITCH |
| 3 | K2 | RATE | 4BAR · 2BAR · 1BAR · 1/2 · 1/4 · 1/8 · 1/16 · 1/32 | 1/8 |
| 3 | K3 | DEPTH | -100 % … +100 % | 0 % |
| 3 | K4 | FORM | SINE · TRI · SAW · SQUARE · S&H | SINE |

#### MIXER (FX button)

| Page | Knob | Parameter | Range / values | Default |
|---|---|---|---|---|
| 1 | K1 | VOL KICK | as on the track page | |
| 1 | K2 | VOL HATS | as on the track page | |
| 1 | K3 | VOL BASS | as on the track page | |
| 1 | K4 | VOL ARP | as on the track page | |
| 2 | K1 | VOL LEAD | as on the track page | |
| 2 | K2 | VOL MEME | as on the track page | |
| 2 | K3 | MASTER | 0 % … 120 % | 80 % |
| 3 | K1 | PAN KICK | -100 % … +100 % | 0 % |
| 3 | K2 | PAN HATS | -100 % … +100 % | 0 % |
| 3 | K3 | PAN BASS | -100 % … +100 % | 0 % |
| 3 | K4 | PAN ARP | -100 % … +100 % | 0 % |
| 4 | K1 | PAN LEAD | -100 % … +100 % | 0 % |
| 4 | K2 | PAN MEME | -100 % … +100 % | 0 % |
| 4 | K3 | E-ZEIT | as on the track page | |
| 4 | K4 | FEEDBK | as on the track page | |
| 5 | K1 | ECHO KICK | 0 % … 100 % | 0 % |
| 5 | K2 | ECHO HATS | 0 % … 100 % | 0 % |
| 5 | K3 | ECHO BASS | 0 % … 100 % | 0 % |
| 5 | K4 | ECHO ARP | 0 % … 100 % | 0 % |
| 6 | K1 | ECHO LEAD | 0 % … 100 % | 0 % |
| 6 | K2 | ECHO MEME | as on the track page | |

#### GLOBAL (GLO button)

| Page | Knob | Parameter | Range / values | Default |
|---|---|---|---|---|
| 1 | K1 | BPM | 60 … 200 | 160 |
| 1 | K2 | SWING | 0 % … 60 % | 0 % |
| 1 | K3 | CRUSH | 0 % … 100 % | 45 % |
| 1 | K4 | DRIVE | 0 % … 100 % | 35 % |

<!-- /GEN:PARAMS -->

## 10. Mixer and global

**FX** opens the mixer (6 pages): the tracks' volumes and MASTER, each track's pan, each track's echo send, echo time and
feedback. The volumes are the same as the VOL parameters on the track pages.

**GLO** opens global: **BPM** (60–200, also by tap tempo on SEL), **SWING**, **CRUSH** (bit depth and sample rate of the
master, for the 8-bit sound) and **DRIVE** (saturation of the master). A compressor always follows.

## 11. The 96 meme sounds

Six banks of 16 sounds. A tile's number is the lower key (1 = leftmost). In PLAY mode on the MEME track, pick the bank
with **OP1–OP6** and play the sounds on the 16 lower keys. **PRESETS** goes through all 96. The code shows in the step grid.

<!-- GEN:MEMES -->
| Key (lower row) | Bank A · CARTOON (OP1) | Bank B · ARCADE (OP2) | Bank C · MEME (OP3) | Bank D · ANIMAL (OP4) | Bank E · SCI-FI (OP5) | Bank F · PARTY (OP6) |
|---|---|---|---|---|---|---|
| 1 (F3) | BOING `BNG` | PEW `PEW` | HORN `HRN` | QUACK `QAK` | ROBOT `ROB` | HORN x3 `HR3` |
| 2 (G3) | SPRING `SPR` | COIN `CON` | BOOM `BOM` | MEOW `MEW` | BEEP `BEP` | SCRATCH `SCR` |
| 3 (A3) | SLIDE UP `SL^` | JUMP `JMP` | FART `FRT` | CUCKOO `CUC` | UFO `UFO` | RISER `RSR` |
| 4 (B3) | SLIDE DN `SLv` | 1UP `1UP` | SAD `SAD` | HONK `HNK` | WARP `WRP` | DROP `DRP` |
| 5 (C4) | BONK `BNK` | POWER `PWR` | RIMSHOT `RIM` | KAZOO `KAZ` | ERROR `ERR` | APPLAUS `APL` |
| 6 (D4) | POP `POP` | HIT `HIT` | WOLF `WLF` | SIREN `SIR` | NOTIF `NTF` | LAUGH `LOL` |
| 7 (E4) | WHIP `WHP` | LASER `LSR` | CRICKET `CRK` | BURP `BRP` | TELEPRT `TLP` | GONG `GNG` |
| 8 (F4) | ZIP `ZIP` | GAMEOVR `G.O` | BRUH `BRH` | BLUB `BLB` | GLITCH `GLT` | TADA `TDA` |
| 9 (G4) | TWANG `TWG` | PIPE `PIP` | OOF `OOF` | CHIRP `CHP` | MODEM `MDM` | CLAP `CLP` |
| 10 (A4) | SQUEAK `SQK` | SELECT `SEL` | DUNDUN `DUN` | FROG `FRG` | SONAR `SON` | SNARE `SNR` |
| 11 (B4) | TIPTOE `TIP` | BOMB `BMB` | BOI `BOI` | DOG `DOG` | ALARM `ALM` | CRASH `CRS` |
| 12 (C5) | PAN `PAN` | PUNCH `PCH` | TAPESTP `TPS` | COW `COW` | POWERDN `PDN` | WHISTLE `WHS` |
| 13 (D5) | BONGO `BGO` | CHARGE `CHG` | EARRAPE `EAR` | BEE `BEE` | BLASTER `BLS` | DUBSIRN `DUB` |
| 14 (E5) | BALLOON `BLN` | VICTORY `VIC` | WOW `WOW` | OWL `OWL` | DATA `DAT` | CHEER `CHR` |
| 15 (F5) | SPLAT `SPL` | DEATH `DTH` | VIBRATE `VIB` | GOAT `GOT` | BEAM `BEM` | COWBELL `CWB` |
| 16 (G5) | HICCUP `HIC` | BUMP `BMP` | DRAMATIC `DRM` | ROOSTER `ROO` | HYPER `HYP` | CONFETTI `CNF` |
<!-- /GEN:MEMES -->

## 12. MIDI

- **Notes** over USB-MIDI or the MIDI jack play the **LEAD** track
- **CC 123** (all notes off) stops hanging notes
- fm1-dock's MIDI options (channel, thru, clock) are in the fm1-dock picker under OPTIONS

## 13. Recipes

- **Classic tek groove:** kick on 1, 5, 9, 13. Bass on the three 16ths between the kicks (dice with ARP on the BASS track
  gives exactly that). Open hats on 3, 7, 11, 15. 160–175 BPM, a little DUCK on bass page 3
- **Memecore drop:** copy P1 to P2 with **EDIT**, add memes in P2 (bank C or F), chain on
- **Wobble bass:** bass LFO1 on CUTOFF, RATE 1/8, DEPTH 60 %, RESO high
- **Hardstyle supersaw:** bass WAVE SUPERSAW, TIMBRE 60 %, WIDTH 80 %, OCT +1, DRIVE 40 %
- **FM growl:** bass WAVE FM, T-ENV +70 %, LFO2 on TIMBRE at 1/16 with S&H
- **8-bit melody:** LEAD track, WAVE PULSE25, ARP DUR (major), enter note by note with REC while stopped, GLOBAL CRUSH 60 %
- **Chaos break:** MEME MODE STUTTER, RND-PITCH 7, ECHO 40 %, flip the live pitch with the upper row
- **Meme siren:** meme LFO on PITCH, RATE 1/2, FORM TRI, DEPTH 40 %, with bank C or F
- **Echo dub:** meme LFO on ECHO, RATE 1BAR, FORM SINE, FEEDBK 60 %

## 14. When something doesn't work

| Problem | Fix |
|---|---|
| No sound | turn MASTER up. Check for muted tracks (grey in the footer) or an active solo (yellow). Check the mixer volumes |
| A step doesn't react | are you in STEP mode? Press SEQ |
| The keys don't play | in PLAY mode they play the selected track, in STEP mode they set steps |
| Saving doesn't work | SAVE works only when stopped |
| Clicks, dropouts | fewer long memes at once, less echo feedback. Please report it (below) |
| Memetek FM1 doesn't start any more | fm1-dock skips a module after two crashes. Hold HOME and pick it again, or drop it on the console again |

Please report bugs and wishes as a [GitHub issue](https://github.com/g0rst/memetek-fm1/issues).

## 15. Versions

| Version | New |
|---|---|
| 0.6 | an LFO for MEME FX (page 3): PITCH, VOL, TONE, PAN, ECHO, LAENGE |
| 0.5 | the name is **Memetek FM1** again; patterns: the first press of PIT/GLO/MONO/POLY shows and edits a pattern, the second activates it (plays from the end of the bar); edits of patterns that aren't active, and in a chain, are heard only once activated; the header shows the edited and the playing pattern |
| 0.4 | name g0dsgift (genre: 8-bit memetek); synthesis for bass, arp, lead: SUPERSAW, FM, PWM, SYNC, FOLD with TIMBRE, T-ENV, WIDTH; bass filter types; LFO target TIMBRE; English manual |
| 0.3 | manual; arp steps show I / IV / V |
| 0.2 | all 96 meme sounds in 6 banks; bank select and live pitch on the upper row; tap tempo (SEL); pattern copy (EDIT); HOME tapped; track shift (OCT in STEP mode) |
| 0.1 | first firmware: 6 tracks, sequencer, 4 patterns, chain, LFOs, mixer, echo, crusher, 16 memes |

## 16. Licence and thanks

Memetek FM1 is free software under the GPL-3.0. **Inspired by g0dsgift777.** Free all Ⓐnarchists!

It builds on [fm1-dock](https://github.com/joakimeriksson/fm1-dock) by Joakim
Eriksson, which builds on Dinghy and Felucca (Leo Kuroshita, Hügelton Instruments) and SLOOP (isod89). It is tested with
[fm1-emu](https://github.com/joakimeriksson/fm1-emu) and built on GitHub with [zvenson/fm1-build](https://github.com/zvenson/fm1-build).
