# MEMETEK – 8-Bit-Tek-Groovebox für das M-VAVE FM-1

Memecore, lustige Sounds und 8-Bit-Klänge, dazu Tek-Kick und rollende Bass-Pattern. MEMETEK läuft als Modul für
[fm1-dock](https://github.com/joakimeriksson/fm1-dock) oder als eigenständige Firmware.

**Status: v0.2. Läuft im FM-1-Emulator, auf echter Hardware noch ungetestet.**

**Ohne Gerät ausprobieren:** https://g0rst.github.io/memetek-fm1/ (dort im Emulator testen)

- 6 Spuren: TEK-KICK, HI-HATS, 8BIT BASS, CHIP ARP, 8BIT LEAD, MEME FX
- 16-Step-Sequencer, 4 Pattern, Chain
- Seiten mit Parametern auf den 4 Knobs, 3 LFOs (2× Bass, Arp, Lead), Mixer (Lautstärke, Pan, Echo), Bitcrusher, Drive
- 96 Meme-Sounds in 6 Bänken (CARTOON, ARCADE, MEME, ANIMAL, SCI-FI, PARTY), live synthetisiert
- Browser-Prototyp: [web/index.html](web/index.html)

## Bedienung am FM-1

| Bedienelement | Funktion |
|---|---|
| **PLAY** | Start / Stop |
| **SEQ** | Umschalten: STEP-Modus (weiße Tasten = 16 Steps) ↔ PLAY-Modus (Tasten spielen die Spur) |
| **REC** | Aufnahme: läuft der Sequencer, wird im Takt aufgenommen, sonst Step für Step |
| **SELECT** (Drehregler) | Spur wählen |
| **ALGORITHM** | Seite wechseln |
| **KNOB 1–4** | die 4 Parameter der Seite |
| **PRESETS** | Meme-Sound wählen (alle 96) |
| **ENV** / **LFO** / **FX** / **GLO** | Spur-Seiten / LFO-Seite / Mixer / Tempo, Swing, Crush, Drive |
| **ARP** | Würfeln: zufälliges Pattern für die Spur · **SEL + ARP**: Spur löschen |
| **SEL** kurz tippen | Tap-Tempo (mehrmals im Takt tippen) |
| **EDIT** | Pattern ins nächste kopieren und dorthin wechseln |
| **HOME** kurz | zurück: erste Seite der Spur, STEP-Modus (lang halten: fm1-dock-Auswahl) |
| **OCT− / OCT+** | STEP-Modus: Spur um einen Step verschieben · PLAY-Modus: Oktave der Tasten |
| **SAVE** | Pattern und Sounds dauerhaft speichern (nur im Stopp) |
| STEP-Modus, obere Reihe **OP1–OP6** | Mute für KICK, HATS, BASS, ARP, LEAD, MEME (mit **SEL** gehalten: Solo) |
| STEP-Modus, obere Reihe **PIT, GLO, MONO, POLY** | Pattern 1–4 |
| STEP-Modus, obere Reihe, **unbeschriftete Taste rechts** | Chain an/aus (spielt die Pattern der Reihe nach) |
| untere Reihe, **16 Tasten** | STEP-Modus: die 16 Steps · PLAY-Modus: spielen (bei MEME: die 16 Sounds der Bank) |
| PLAY-Modus, MEME-Spur, **OP1–OP6** | Bank A–F wählen |
| PLAY-Modus, MEME-Spur, **PIT, GLO, MONO, POLY, rechte Taste** | Tonhöhe der Memes −12, −5, 0, +7, +12 |
| **MASTER** | Gesamtlautstärke |

Steps antippen schaltet durch: Kick aus/an/Akzent · Hats aus/zu/offen/Roll · Bass und Arp aus/Grundton/Quinte/Oktave ·
Meme setzt den gewählten Sound · Lead setzt eine Note (zweimal antippen löscht sie).

## Installieren

Du brauchst Chrome oder Edge an einem Computer und ein USB-Datenkabel.

1. **fm1-dock installieren:** auf der [fm1-dock-Seite](https://joakimeriksson.github.io/fm1-dock/#install) „Connect over USB“ und
   dann „Install fm1-dock“ wählen.
2. **MEMETEK laden:** im neuesten [Build](../../actions) unter „Artifacts“ das Paket `packages` herunterladen, entpacken und
   `memetek.dock` auf die [Konsole](https://joakimeriksson.github.io/fm1-dock/#console) ziehen.
3. Am FM-1 **HOME** halten und MEMETEK auswählen.

Zurück zur Original-Firmware kommst du über den [FM-1 Firmware Hub](https://fm1.designburgapps.com) oder M-VAVEs Updater.
Das Installieren geschieht auf eigene Gefahr.

## Bauen und testen

- `tests/run_host.sh` lässt das Modul auf dem Computer laufen, mit Sanitizern. Ergebnis sind `build/host/memetek.wav` und Bildschirmfotos.
- `./build.sh` baut `build/memetek.dock` und `build/memetek.fwsc`. Dafür braucht es die JieLi-Toolchain, deshalb übernimmt das GitHub mit
  [zvenson/fm1-build](https://github.com/zvenson/fm1-build) (siehe `.github/workflows/build.yml`).

## Lizenz

GPL-3.0-only. Aufgebaut auf fm1-dock (Joakim Eriksson), Dinghy/Felucca (Leo Kuroshita, Hügelton Instruments) und SLOOP (isod89).
