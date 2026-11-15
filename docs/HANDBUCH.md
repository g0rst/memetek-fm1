# Memetek FM1 – Handbuch

<!-- GEN:VERSION -->
**Version 0.5**
<!-- /GEN:VERSION -->
 · die Groovebox für **8-Bit-Memetek** auf dem M-VAVE FM-1 · [English manual](manual.html) · [Seite mit Emulator](https://g0rst.github.io/memetek-fm1/) · [Quellcode](https://github.com/g0rst/memetek-fm1)

> Dieses Handbuch gehört zur Firmware und wird mit jeder neuen Version aktualisiert. Die Tabellen mit Parametern und
> Sounds werden direkt aus dem Firmware-Code erzeugt.
> Memetek FM1 ist noch jung und wurde bisher nur im FM-1-Emulator getestet. Das Installieren auf echter Hardware geschieht auf eigene Gefahr.

[TOC]

## 1. Was ist Memetek FM1?

**8-Bit-Memetek** ist das Genre: Tek-Kicks und rollende Bässe, 8-Bit-Klänge alter Spielkonsolen und Meme-Sounds,
laut, schnell und albern. **memetek-fm1** ist das Gerät dafür. Es macht aus dem FM-1 eine Groovebox für genau diese Musik:

- **6 Spuren:** TEK-KICK, HI-HATS, 8BIT BASS, CHIP ARP, 8BIT LEAD, MEME FX
- **16-Step-Sequencer** mit 4 Pattern, Chain, Kopieren, Verschieben, Würfeln und Live-Aufnahme
- **96 Meme-Sounds** in 6 Bänken, jeder Sound wird live erzeugt (keine Samples)
- **Synthese:** Square, Pulse, Saw, Triangle, **Supersaw**, FM, PWM, Sync und Fold für Bass, Arp und Lead
- **Klangbearbeitung über Seiten:** Die 4 Knobs regeln immer die 4 Parameter der aktuellen Seite, dazu 4 LFOs (2× Bass, Arp, Lead)
- **Mixer** mit Lautstärke, Panorama und Echo für jede Spur, außerdem Bitcrusher, Drive und Kompressor für die Summe

## 2. Installieren

### Ohne Gerät ausprobieren

Auf der [Memetek FM1-Seite](https://g0rst.github.io/memetek-fm1/) auf **„Im FM-1-Emulator testen“** tippen. Dort läuft
dieselbe Firmware wie auf dem Gerät, in einem nachgebauten FM-1. Auf alten Computern kann der Emulator zu langsam sein, auf
einem aktuellen Handy läuft er gut.

### Auf das FM-1

Du brauchst Chrome oder Edge an einem Computer und ein USB-Datenkabel.

1. **fm1-dock installieren** (nur einmal): Auf der [fm1-dock-Seite](https://joakimeriksson.github.io/fm1-dock/#install)
   „Connect over USB“ und dann „Install fm1-dock“ wählen.
2. **Memetek FM1 laden:** Im [neuesten Release](https://github.com/g0rst/memetek-fm1/releases) `memetek-fm1.dock` herunterladen und auf die
   [fm1-dock-Konsole](https://joakimeriksson.github.io/fm1-dock/#console) ziehen.
3. Am FM-1 **HOME** etwa 1,5 Sekunden halten und **MEMETEK FM1** auswählen.

Alternativ ist `memetek-fm1.fwsc` dieselbe Firmware als eigenständiges Paket, also fm1-dock mit nur Memetek FM1.

**Updates:** Die neue `memetek-fm1.dock` einfach erneut auf die Konsole ziehen. Heißt das Modul bei dir noch G0DSGIFT oder MEMETEK, lösch das alte in der fm1-dock-Konsole.

**Zurück zur Original-Firmware:** über den [FM-1 Firmware Hub](https://fm1.designburgapps.com) oder M-VAVEs eigenen Updater.
Ein Modul, das zweimal hintereinander abstürzt, überspringt fm1-dock beim Start automatisch.

## 3. Das Bedienfeld

```
  MASTER   SELECT     ┌──────────┐    KNOB1   KNOB2   KNOB3   KNOB4
                      │ Display  │
  PRESETS  ALGORITHM  │          │    [FX]  [SEL] [ENV] [LFO] [EDIT] [GLO]
                      └──────────┘    [HOME][SAVE][ARP] [SEQ] [PLAY] [REC]
  [OCT-] [OCT+]

  obere Reihe:  OP1 OP2 OP3   OP4 OP5   OP6 PIT GLO   MONO POLY   (rechts)
  untere Reihe: 16 Tasten = 16 Steps
```

| Element | Funktion |
|---|---|
| **MASTER** | Gesamtlautstärke |
| **SELECT** | Spur wählen (KICK → HATS → BASS → ARP → LEAD → MEME) |
| **ALGORITHM** | Seite der aktuellen Spur wechseln |
| **PRESETS** | Meme-Sound wählen (alle 96). Im Stopp hörst du ihn sofort |
| **KNOB 1–4** | die 4 Parameter der Seite, Anzeige K1–K4 auf dem Display |
| **PLAY/STOP** | Sequencer starten oder stoppen |
| **REC** | Aufnahme an/aus (siehe [Aufnehmen](#7-aufnehmen)) |
| **SEQ** | Umschalten zwischen STEP-Modus und PLAY-Modus |
| **ENV** | zu den Seiten der aktuellen Spur |
| **LFO** | direkt zur LFO-Seite der Spur (Bass, Arp, Lead) |
| **FX** | Mixer |
| **GLO** | Global: Tempo, Swing, Crush, Drive |
| **EDIT** | Pattern ins nächste kopieren und dorthin wechseln |
| **ARP** | Würfeln: zufälliges, passendes Pattern für die aktuelle Spur |
| **SEL + ARP** | aktuelle Spur im Pattern löschen |
| **SEL** kurz tippen | Tap-Tempo: mehrmals im Takt tippen |
| **SAVE** | alles dauerhaft speichern (nur im Stopp) |
| **HOME** kurz | zurück: erste Seite der Spur, STEP-Modus |
| **HOME** lang (1,5 s) | fm1-dock-Auswahl (andere Synths) |
| **OCT− / OCT+** | STEP-Modus: Spur um einen Step verschieben · PLAY-Modus: Oktave der Tasten |

## 4. Das Display

- **Kopfzeile:** links MEMETEK FM1 und das **bearbeitete** Pattern (`P2*` = noch nicht aktivierte Änderungen), rechts das
  **spielende** Pattern mit `>` (läuft) oder `||` (Stopp), `C` bei Chain, und das Tempo, z. B. `>P1 160`
- **Titel:** die Spur in ihrer Farbe (oder MIXER / GLOBAL), rechts die Seite, z. B. `S2/5`
- **Step-Raster:** 16 Felder in zwei Reihen. Farbig ist ein gesetzter Step, weiß der gerade gespielte. Das Kürzel im Feld
  zeigt den Wert (siehe unten). Bei LEAD markiert ein Strich den Cursor für die Aufnahme
- **Parameter:** die 4 Parameter der Seite mit Wert und Balken (K1–K4 = KNOB 1–4)
- **Fußzeile:** die Spuren `K H B A L M` (grau = gemutet, gelb = Solo, hinterlegt = gewählt), dazu Modus, aktueller
  Meme-Sound mit Bank (z. B. `F:APPLAUS`) und die Live-Tonhöhe. Rot während der Aufnahme. Kurzmeldungen erscheinen hier gelb

## 5. STEP-Modus und PLAY-Modus

**SEQ** schaltet zwischen den beiden Modi. Die Tasten-LEDs zeigen den Modus: Im STEP-Modus leuchten die gesetzten Steps
und der Lauflicht-Step.

### STEP-Modus: programmieren

- **Untere 16 Tasten:** Steps der aktuellen Spur setzen. Mehrmals tippen schaltet durch die Werte:

| Spur | Werte beim Tippen | Anzeige |
|---|---|---|
| KICK | aus → an → Akzent | leer, farbig, `!` |
| HATS | aus → zu → offen → Roll | `x`, `O`, `R` |
| BASS | aus → Grundton → Quinte → Oktave | `1`, `5`, `8` |
| ARP | aus → Akkord auf Stufe I → IV → V | `I`, `IV`, `V` |
| LEAD | leere Stelle: setzt eine Note (Grundton der Tonart) · gesetzte Stelle am Cursor: löscht sie | Notenname |
| MEME | setzt den gewählten Meme-Sound · ist er schon gesetzt, wird er gelöscht | 3-Buchstaben-Kürzel |

- **Obere Reihe:**

| Taste | Funktion |
|---|---|
| **OP1–OP6** | Mute für KICK, HATS, BASS, ARP, LEAD, MEME |
| **SEL + OP1–OP6** | Solo (mehrere möglich). Mit Solo laufen nur die Solo-Spuren |
| **PIT, GLO, MONO, POLY** | Pattern 1–4. **Erster Druck:** das Pattern anzeigen und bearbeiten, das bisherige spielt weiter. **Zweiter Druck:** aktivieren, es spielt ab dem Taktende (siehe [Pattern](#6-pattern-chain-kopieren-wurfeln)) |
| **rechte Taste** | Chain an/aus |

- **OCT− / OCT+** verschieben die Spur um einen Step nach links oder rechts.

### PLAY-Modus: spielen

Die Tasten spielen die aktuelle Spur:

| Spur | untere Reihe | obere Reihe |
|---|---|---|
| KICK | Kick | Kick mit Akzent |
| HATS | geschlossen | offen |
| BASS, LEAD | chromatisch spielbar (F3–G5, mit OCT verschiebbar) | chromatisch |
| ARP | Arpeggio | Arpeggio |
| MEME | die 16 Sounds der aktuellen Bank | **OP1–OP6:** Bank A–F · **PIT, GLO, MONO, POLY, rechts:** Tonhöhe −12, −5, 0, +7, +12 |

Die Live-Tonhöhe der Memes gilt auch für die Memes im laufenden Pattern. So lassen sich ganze Breaks transponieren.

## 6. Pattern, Chain, Kopieren, Würfeln

- **4 Pattern** (P1–P4) enthalten jeweils alle 6 Spuren. Im STEP-Modus mit **PIT, GLO, MONO, POLY**:
  - **Erster Druck** auf einen Pattern-Knopf: Das Display springt auf dieses Pattern, alle Änderungen gehen nur dorthin.
    Das bisher laufende Pattern spielt unverändert weiter, deine Änderungen sind also noch nicht zu hören. Die Kopfzeile zeigt
    `P2*`, wenn es noch nicht aktivierte Änderungen gibt. Die LED des bearbeiteten Patterns blinkt
  - **Zweiter Druck** auf denselben Knopf: Das Pattern wird **aktiviert**. Die Änderungen werden übernommen, und es spielt ab
    dem nächsten Taktende. Die LED des spielenden Patterns leuchtet
  - Bearbeitest du das gerade aktive Pattern, hörst du jede Änderung sofort
- **Chain:** Mit der rechten Taste in der oberen Reihe spielt Memetek FM1 nach jedem Takt das nächste nicht-leere Pattern.
  Ein geändertes Pattern spielt in der Chain seine alte Fassung weiter, bis du es mit seinem Knopf aktivierst (bei
  angezeigtem Pattern ein Druck)
- **EDIT** kopiert das aktuelle Pattern ins nächste (P1 → P2 …) und zeigt es an. Ein Druck auf seinen Knopf aktiviert es
- **ARP** würfelt ein Pattern für die aktuelle Spur: Kick auf den Vierteln, offene Hats auf den Offbeats, ein rollender Bass
  zwischen den Kicks, Memes aus der aktuellen Bank
- **SEL + ARP** löscht die aktuelle Spur
- **OCT− / OCT+** im STEP-Modus verschieben die Spur

**Werkseinstellung:** P1 Tek · P2 Memecore · P3 Chiptune mit Melodie · P4 Meme-Break

## 7. Aufnehmen

1. Spur wählen und mit **SEQ** in den **PLAY-Modus** wechseln
2. **REC** drücken. Die Fußzeile wird rot
3. **Läuft der Sequencer:** Was du spielst, wird auf den aktuellen Step geschrieben
4. **Im Stopp:** Jede Taste schreibt auf den Cursor, der dann einen Step weiterspringt (Step-Eingabe). Bei LEAD setzt du den
   Cursor im STEP-Modus (mit REC an) durch Tippen auf einen Step
5. **REC** erneut beendet die Aufnahme

Aufgenommen werden: KICK (obere Reihe = Akzent), HATS (obere Reihe = offen), BASS und ARP (Grundton), LEAD (die gespielte
Note), MEME (der gespielte Sound).

## 8. Speichern

**SAVE** (nur im Stopp) speichert alle 4 Pattern, alle Parameter, den Mixer und die Chain im Flash des FM-1. Beim nächsten
Start von Memetek FM1 ist alles wieder da. Beim Speichern setzt der Ton kurz aus, das ist normal.

## 9. Die Spuren und ihre Parameter

Mit **SELECT** die Spur wählen, mit **ALGORITHM** die Seite. **KNOB 1–4** regeln die 4 Parameter der Seite. **LFO**
springt direkt zur LFO-Seite.

- **TEK-KICK:** gepitchte, verzerrte Kick. TUNE ist der Endton, DROP und DROP-T bestimmen den Pitch-Abfall, DECAY den
  Ausklang. DIST wählt die Verzerrung: SOFT, HARD oder FOLD (Wavefolder, metallisch)
- **HI-HATS:** 5 Typen (METAL, NOISE, 8-BIT, SHAKER, RIDE). ROLL legt fest, wie viele Schläge ein Roll-Step hat. HUMANIZE
  variiert die Lautstärke, CHANCE die Wahrscheinlichkeit jedes Schlags. ACCENT betont die Offbeats, STEREO verteilt die Hats im
  Panorama, CHOKE lässt geschlossene Hats offene abwürgen
- **8BIT BASS:** Oszillator (siehe [Synthese](#synthese)) mit Sub und resonantem Filter (LOWPASS, BANDPASS, HIGHPASS auf
  Seite 4). F-ENV ist die Filter-Hüllkurve, DECAY die Tonlänge in Steps. **ROOT ist die Tonart für Bass, Arp und
  Würfel-Melodien.** DUCK duckt den Bass bei jeder Kick (Pumpen). Seiten 5 und 6 sind zwei LFOs
- **CHIP ARP:** Gameboy-Arpeggio pro Step. RATE ist die Zahl der Noten pro Step, RICHTG die Richtung, AKKORD Moll, Dur, Sus
  oder Power. Seite 3 ist der LFO, Seite 4 die Synthese
- **8BIT LEAD:** monophone Melodiestimme mit Square, Pulse, Triangle, Saw und einem NOISE-Kanal wie bei alten Konsolen. Dazu
  LAENGE in Steps, GLIDE, SWEEP (Tonhöhe gleitet beim Anschlag), DETUNE, Hüllkurve und Chip-Arpeggio. Seite 4 ist der LFO,
  Seite 5 die Synthese. Dazu gibt es alle Synthese-Arten des Basses
- **MEME FX:** PITCH transponiert alle Memes, LAENGE verkürzt und beschleunigt sie. MODE STUTTER spielt vier kurze Schläge
  pro halbem Step. RND-PITCH verstimmt jeden Schlag zufällig. ECHO, E-ZEIT und FEEDBK steuern das Echo

### Synthese

Bass, Arp und Lead haben denselben Oszillator. **WAVE** wählt die Synthese-Art, **TIMBRE** formt sie:

| WAVE | Klang | TIMBRE regelt |
|---|---|---|
| SQUARE, PULSE, SAW, TRI | die klassischen 8-Bit-Wellen, bewusst ungefiltert und rau | (nichts) |
| **SUPERSAW** | sieben gegeneinander verstimmte Sägezähne: breit, fett, Trance und Hardstyle | die Verstimmung |
| **FM** | Zwei-Operator-FM, der Modulator eine Oktave höher: Glocken, Growl, metallisch | den Modulationsindex |
| **PWM** | Rechteck mit wandernder Pulsbreite: schwebend, chorusartig | die Tiefe der Bewegung |
| **SYNC** | Hard-Sync-Sägezahn: schneidend, Sweeps klingen nach Laser | die Tonhöhe des Slaves |
| **FOLD** | Sinus durch einen Wavefolder: von rund bis kreischend | den Drive |
| PULSE12, NOISE (nur Lead) | dünner Puls, Noise-Kanal alter Konsolen | (nichts) |

- **T-ENV:** Hüllkurve auf TIMBRE pro Ton. Positiv startet hell und wird dunkler, negativ umgekehrt. Damit „wow“t ein FM-Bass
  oder ein Supersaw öffnet sich
- **WIDTH:** Stereobreite des Supersaw
- **FILTER** (nur Bass): Filtertyp LOWPASS, BANDPASS oder HIGHPASS
- Die LFOs können **TIMBRE** als Ziel haben: Supersaw-Schweben, FM-Wobble, Fold-Bewegung

### LFOs

Bass (2×), Arp und Lead haben LFOs mit **ZIEL** (was moduliert wird, auch TIMBRE), **RATE** (im Takt: 4 Takte bis 1/32), **DEPTH**
(Tiefe in beide Richtungen, negativ = umgekehrt) und **FORM** (Sinus, Dreieck, Sägezahn, Rechteck, S&H = Zufall pro Periode).
Die LFOs laufen synchron zum Takt und starten mit PLAY neu.

### Alle Parameter

<!-- GEN:PARAMS -->
#### TEK-KICK

| Seite | Knob | Parameter | Bereich / Werte | Start |
|---|---|---|---|---|
| 1 | K1 | TUNE | 30 Hz … 80 Hz | 42 Hz |
| 1 | K2 | DECAY | 0,15 s … 0,9 s | 0,45 s |
| 1 | K3 | DROP | x2 … x10 | x5 |
| 1 | K4 | DROP-T | 10 ms … 150 ms | 60 ms |
| 2 | K1 | DRIVE | 0 % … 100 % | 30 % |
| 2 | K2 | DIST | SOFT · HARD · FOLD | SOFT |
| 2 | K3 | CLICK | 0 % … 100 % | 60 % |
| 2 | K4 | VOL | 0 % … 100 % | 90 % |

#### HI-HATS

| Seite | Knob | Parameter | Bereich / Werte | Start |
|---|---|---|---|---|
| 1 | K1 | TUNE | x0,5 … x2 | x1 |
| 1 | K2 | ZU | 10 ms … 100 ms | 35 ms |
| 1 | K3 | OFFEN | 80 ms … 600 ms | 220 ms |
| 1 | K4 | VOL | 0 % … 100 % | 30 % |
| 2 | K1 | TYP | METAL · NOISE · 8-BIT · SHAKER · RIDE | METAL |
| 2 | K2 | ROLL | 2x · 3x · 4x · 6x | 3x |
| 2 | K3 | HUMANIZE | 0 % … 100 % | 0 % |
| 2 | K4 | CHANCE | 0 % … 100 % | 100 % |
| 3 | K1 | TONE | x0,3 … x1,5 | x1 |
| 3 | K2 | ACCENT | 0 % … 100 % | 0 % |
| 3 | K3 | STEREO | 0 % … 100 % | 0 % |
| 3 | K4 | CHOKE | AUS · AN | AN |

#### 8BIT BASS

| Seite | Knob | Parameter | Bereich / Werte | Start |
|---|---|---|---|---|
| 1 | K1 | CUTOFF | 100 Hz … 5000 Hz | 2400 Hz |
| 1 | K2 | RESO | 0,5 … 20 | 6 |
| 1 | K3 | F-ENV | 0 % … 100 % | 80 % |
| 1 | K4 | DECAY | x0,2 … x1,5 | x0,9 |
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

| Seite | Knob | Parameter | Bereich / Werte | Start |
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

| Seite | Knob | Parameter | Bereich / Werte | Start |
|---|---|---|---|---|
| 1 | K1 | WAVE | SQUARE · PULSE25 · PULSE12 · TRI · SAW · NOISE · SUPERSAW · FM · PWM · SYNC · FOLD | SQUARE |
| 1 | K2 | LAENGE | x0,25 … x4 | x1 |
| 1 | K3 | VIBRATO | 0 % … 100 % | 0 % |
| 1 | K4 | VOL | 0 % … 100 % | 35 % |
| 2 | K1 | OCT | -2 … +2 | 0 |
| 2 | K2 | GLIDE | 0 ms … 300 ms | 0 ms |
| 2 | K3 | SWEEP | -12 … +12 | 0 |
| 2 | K4 | DETUNE | 0 % … 100 % | 0 % |
| 3 | K1 | ATTACK | 0 ms … 300 ms | 0 ms |
| 3 | K2 | DECAY | 0,05 s … 2 s | 0,4 s |
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

| Seite | Knob | Parameter | Bereich / Werte | Start |
|---|---|---|---|---|
| 1 | K1 | PITCH | -12 … +12 | 0 |
| 1 | K2 | LAENGE | 10 % … 100 % | 100 % |
| 1 | K3 | MODE | NORMAL · STUTTER | NORMAL |
| 1 | K4 | VOL | 0 % … 100 % | 50 % |
| 2 | K1 | RND-PITCH | 0 … 12 | 0 |
| 2 | K2 | ECHO | 0 % … 100 % | 0 % |
| 2 | K3 | E-ZEIT | 1/16 · 1/8 · 3/16 · 1/4 | 1/8 |
| 2 | K4 | FEEDBK | 0 % … 90 % | 40 % |

#### MIXER (Taste FX)

| Seite | Knob | Parameter | Bereich / Werte | Start |
|---|---|---|---|---|
| 1 | K1 | VOL KICK | wie auf der Spur-Seite | |
| 1 | K2 | VOL HATS | wie auf der Spur-Seite | |
| 1 | K3 | VOL BASS | wie auf der Spur-Seite | |
| 1 | K4 | VOL ARP | wie auf der Spur-Seite | |
| 2 | K1 | VOL LEAD | wie auf der Spur-Seite | |
| 2 | K2 | VOL MEME | wie auf der Spur-Seite | |
| 2 | K3 | MASTER | 0 % … 120 % | 80 % |
| 3 | K1 | PAN KICK | -100 % … +100 % | 0 % |
| 3 | K2 | PAN HATS | -100 % … +100 % | 0 % |
| 3 | K3 | PAN BASS | -100 % … +100 % | 0 % |
| 3 | K4 | PAN ARP | -100 % … +100 % | 0 % |
| 4 | K1 | PAN LEAD | -100 % … +100 % | 0 % |
| 4 | K2 | PAN MEME | -100 % … +100 % | 0 % |
| 4 | K3 | E-ZEIT | wie auf der Spur-Seite | |
| 4 | K4 | FEEDBK | wie auf der Spur-Seite | |
| 5 | K1 | ECHO KICK | 0 % … 100 % | 0 % |
| 5 | K2 | ECHO HATS | 0 % … 100 % | 0 % |
| 5 | K3 | ECHO BASS | 0 % … 100 % | 0 % |
| 5 | K4 | ECHO ARP | 0 % … 100 % | 0 % |
| 6 | K1 | ECHO LEAD | 0 % … 100 % | 0 % |
| 6 | K2 | ECHO MEME | wie auf der Spur-Seite | |

#### GLOBAL (Taste GLO)

| Seite | Knob | Parameter | Bereich / Werte | Start |
|---|---|---|---|---|
| 1 | K1 | BPM | 60 … 200 | 160 |
| 1 | K2 | SWING | 0 % … 60 % | 0 % |
| 1 | K3 | CRUSH | 0 % … 100 % | 45 % |
| 1 | K4 | DRIVE | 0 % … 100 % | 35 % |

<!-- /GEN:PARAMS -->

## 10. Mixer und Global

**FX** öffnet den Mixer (6 Seiten): Lautstärke der Spuren und MASTER, Panorama jeder Spur, Echo-Anteil (Send) jeder Spur sowie
Echo-Zeit und Feedback. Die Lautstärken sind dieselben wie die VOL-Parameter auf den Spur-Seiten.

**GLO** öffnet Global: **BPM** (60–200, auch per Tap-Tempo auf SEL), **SWING**, **CRUSH** (Bittiefe und Samplerate der Summe,
für den 8-Bit-Klang) und **DRIVE** (Sättigung der Summe). Danach folgt immer ein Kompressor.

## 11. Die 96 Meme-Sounds

Sechs Bänke zu je 16 Sounds. Die Nummer der Kachel ist die untere Taste (1 = ganz links). Im PLAY-Modus der MEME-Spur wählst du die Bank mit **OP1–OP6** und spielst die Sounds auf den
16 unteren Tasten. **PRESETS** blättert durch alle 96. Das Kürzel erscheint im Step-Raster.

<!-- GEN:MEMES -->
| Taste (unten) | Bank A · CARTOON (OP1) | Bank B · ARCADE (OP2) | Bank C · MEME (OP3) | Bank D · ANIMAL (OP4) | Bank E · SCI-FI (OP5) | Bank F · PARTY (OP6) |
|---|---|---|---|---|---|---|
| 1 (F3) | BOING `BNG` | PEW `PEW` | HORN `HRN` | QUACK `QAK` | ROBOT `ROB` | HORN x3 `HR3` |
| 2 (G3) | SPRING `SPR` | COIN `CON` | BOOM `BOM` | MEOW `MEW` | BEEP `BEP` | SCRATCH `SCR` |
| 3 (A3) | SLIDE UP `SL^` | JUMP `JMP` | FART `FRT` | CUCKOO `CUC` | UFO `UFO` | RISER `RSR` |
| 4 (H3) | SLIDE DN `SLv` | 1UP `1UP` | SAD `SAD` | HONK `HNK` | WARP `WRP` | DROP `DRP` |
| 5 (C4) | BONK `BNK` | POWER `PWR` | RIMSHOT `RIM` | KAZOO `KAZ` | ERROR `ERR` | APPLAUS `APL` |
| 6 (D4) | POP `POP` | HIT `HIT` | WOLF `WLF` | SIREN `SIR` | NOTIF `NTF` | LAUGH `LOL` |
| 7 (E4) | WHIP `WHP` | LASER `LSR` | CRICKET `CRK` | BURP `BRP` | TELEPRT `TLP` | GONG `GNG` |
| 8 (F4) | ZIP `ZIP` | GAMEOVR `G.O` | BRUH `BRH` | BLUB `BLB` | GLITCH `GLT` | TADA `TDA` |
| 9 (G4) | TWANG `TWG` | PIPE `PIP` | OOF `OOF` | CHIRP `CHP` | MODEM `MDM` | CLAP `CLP` |
| 10 (A4) | SQUEAK `SQK` | SELECT `SEL` | DUNDUN `DUN` | FROG `FRG` | SONAR `SON` | SNARE `SNR` |
| 11 (H4) | TIPTOE `TIP` | BOMB `BMB` | BOI `BOI` | DOG `DOG` | ALARM `ALM` | CRASH `CRS` |
| 12 (C5) | PAN `PAN` | PUNCH `PCH` | TAPESTP `TPS` | COW `COW` | POWERDN `PDN` | WHISTLE `WHS` |
| 13 (D5) | BONGO `BGO` | CHARGE `CHG` | EARRAPE `EAR` | BEE `BEE` | BLASTER `BLS` | DUBSIRN `DUB` |
| 14 (E5) | BALLOON `BLN` | VICTORY `VIC` | WOW `WOW` | OWL `OWL` | DATA `DAT` | CHEER `CHR` |
| 15 (F5) | SPLAT `SPL` | DEATH `DTH` | VIBRATE `VIB` | GOAT `GOT` | BEAM `BEM` | COWBELL `CWB` |
| 16 (G5) | HICCUP `HIC` | BUMP `BMP` | DRAMATIC `DRM` | ROOSTER `ROO` | HYPER `HYP` | CONFETTI `CNF` |
<!-- /GEN:MEMES -->

## 12. MIDI

- **Noten** über USB-MIDI oder die MIDI-Buchse spielen die **LEAD**-Spur
- **CC 123** (All Notes Off) stoppt hängende Töne
- Die MIDI-Optionen von fm1-dock (Kanal, Thru, Clock) findest du in der fm1-dock-Auswahl unter OPTIONS

## 13. Rezepte

- **Klassischer Tek-Groove:** Kick auf 1, 5, 9, 13. Bass auf den drei 16teln zwischen den Kicks (Würfeln mit ARP auf der
  BASS-Spur ergibt genau das). Offene Hats auf 3, 7, 11, 15. BPM 160–175, im Mixer etwas DUCK auf der Bass-Seite 3
- **Memecore-Drop:** P1 mit **EDIT** nach P2 kopieren, in P2 Memes dazu (Bank C oder F), Chain an
- **Wobble-Bass:** Bass-LFO1 auf CUTOFF, RATE 1/8, DEPTH 60 %, RESO hoch
- **Hardstyle-Supersaw:** Bass WAVE SUPERSAW, TIMBRE 60 %, WIDTH 80 %, OCT +1, DRIVE 40 %
- **FM-Growl:** Bass WAVE FM, T-ENV +70 %, LFO2 auf TIMBRE mit 1/16 und S&H
- **8-Bit-Melodie:** LEAD-Spur, WAVE PULSE25, ARP DUR, im Stopp mit REC Note für Note eingeben, GLOBAL CRUSH 60 %
- **Chaos-Break:** MEME MODE STUTTER, RND-PITCH 7, ECHO 40 %, Live-Tonhöhe mit der oberen Reihe umschalten

## 14. Wenn etwas nicht klappt

| Problem | Lösung |
|---|---|
| Kein Ton | MASTER hochdrehen. Prüfen, ob Spuren gemutet sind (Fußzeile grau) oder Solo aktiv ist (gelb). Mixer-Lautstärken prüfen |
| Ein Step reagiert nicht | Bist du im STEP-Modus? SEQ drücken |
| Tasten spielen nicht | Im PLAY-Modus spielen sie die gewählte Spur, im STEP-Modus setzen sie Steps |
| Speichern geht nicht | SAVE funktioniert nur im Stopp |
| Knackser, Aussetzer | Weniger lange Memes gleichzeitig, Echo-Feedback senken. Bitte melden (siehe unten) |
| Memetek FM1 startet nicht mehr | fm1-dock überspringt ein Modul nach zwei Abstürzen. HOME halten und es erneut wählen, oder neu auf die Konsole ziehen |

Fehler und Wünsche bitte als [Issue auf GitHub](https://github.com/g0rst/memetek-fm1/issues) melden.

## 15. Versionen

| Version | Neu |
|---|---|
| 0.5 | Name wieder **Memetek FM1**; Pattern: erster Druck auf PIT/GLO/MONO/POLY zeigt und bearbeitet das Pattern, der zweite aktiviert es (spielt ab Taktende); Änderungen an nicht aktiven Pattern und in der Chain sind erst nach dem Aktivieren zu hören; Kopfzeile zeigt bearbeitetes und spielendes Pattern |
| 0.4 | Name g0dsgift (Genre: 8-Bit-Memetek); Synthese für Bass, Arp, Lead: SUPERSAW, FM, PWM, SYNC, FOLD mit TIMBRE, T-ENV, WIDTH; Bass-Filtertypen; LFO-Ziel TIMBRE; englisches Handbuch |
| 0.3 | Handbuch; Arp-Steps zeigen I / IV / V |
| 0.2 | alle 96 Meme-Sounds in 6 Bänken; Bankwahl und Live-Tonhöhe auf der oberen Reihe; Tap-Tempo (SEL); Pattern kopieren (EDIT); HOME kurz; Spur verschieben (OCT im STEP-Modus) |
| 0.1 | erste Firmware: 6 Spuren, Sequencer, 4 Pattern, Chain, LFOs, Mixer, Echo, Crusher, 16 Memes |

## 16. Lizenz und Dank

Memetek FM1 ist freie Software unter der GPL-3.0. **Inspiriert durch g0dsgift777.** Free all Ⓐnarchists!

Es baut auf [fm1-dock](https://github.com/joakimeriksson/fm1-dock) von Joakim
Eriksson auf, das wiederum auf Dinghy und Felucca (Leo Kuroshita, Hügelton Instruments) und SLOOP (isod89) basiert. Getestet wird
mit [fm1-emu](https://github.com/joakimeriksson/fm1-emu). Gebaut wird auf GitHub mit
[zvenson/fm1-build](https://github.com/zvenson/fm1-build).
