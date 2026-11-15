#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""The manual: docs/HANDBUCH.md, with its tables made from the firmware's own source, so they stay true.

  tools/handbuch.py                 fill the tables in docs/HANDBUCH.md (between the <!-- GEN:name --> markers)
  tools/handbuch.py --html OUT.html also write the manual as one web page (needs python-markdown)
  tools/handbuch.py --check         fail when docs/HANDBUCH.md is not up to date with the source

The tables: every track's pages and parameters (modules/memetek/memetek.c, the pdef_t arrays), the 96 memes
(modules/memetek/memes.inc) and the version (VERSION)."""
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SRC = (ROOT / "modules/memetek/memetek.c").read_text()
MEMES = (ROOT / "modules/memetek/memes.inc").read_text()
DOC = ROOT / "docs/HANDBUCH.md"
VERSION = (ROOT / "VERSION").read_text().strip()

LISTS = {m.group(1): re.findall(r'"([^"]*)"', m.group(2))
         for m in re.finditer(r"static const char \*const (L_\w+)\[\] = \{([^}]*)\}", SRC)}


def num(s):
    s = s.strip().rstrip("f")
    return float(s)


def de(x, nd=2):
    s = f"{x:.{nd}f}"
    if "." in s:
        s = s.rstrip("0").rstrip(".")
    if s in ("-0", ""):
        s = "0"
    return s.replace(".", ",")


def fmt(v, f):
    if f == "F_HZ":
        return f"{de(v, 0)} Hz"
    if f == "F_S":
        return f"{de(v)} s"
    if f == "F_MS":
        return f"{de(v * 1000, 0)} ms"
    if f == "F_PCT":
        return f"{de(v * 100, 0)} %"
    if f == "F_PM":
        return f"{'+' if v > 0 else ''}{de(v * 100, 0)} %"
    if f == "F_X":
        return f"x{de(v, 2)}"
    if f == "F_ST":
        return f"{'+' if v > 0 else ''}{de(v, 0)}"
    if f == "F_Q":
        return de(v, 1)
    return de(v, 0)


def parse_defs(name):
    body = re.search(rf"static const pdef_t {name}\[\] = \{{(.*?)\}};", SRC, re.S).group(1)
    out = []
    for m in re.finditer(r'\b(P|PL|PA|PN)\b(?:\(([^()]*)\))?', body):
        kind, args = m.group(1), m.group(2)
        if kind == "PN":
            out.append(None)
            continue
        a = [x.strip() for x in args.split(",")]
        name_ = a[0].strip('"')
        if kind == "P":
            lo, hi, st, d, f = num(a[1]), num(a[2]), num(a[3]), num(a[4]), a[5]
            out.append({"name": name_, "range": f"{fmt(lo, f)} … {fmt(hi, f)}", "def": fmt(d, f)})
        elif kind == "PL":
            lst = LISTS[a[3]]
            out.append({"name": name_, "range": " · ".join(lst), "def": lst[int(num(a[2]))]})
        else:
            out.append({"name": name_, "alias": (a[1], a[2])})
    return out


TRACKS = [("TEK-KICK", "D_KICK"), ("HI-HATS", "D_HAT"), ("8BIT BASS", "D_BASS"), ("CHIP ARP", "D_ARP"),
          ("8BIT LEAD", "D_LEAD"), ("MEME FX", "D_MEME"), ("MIXER (Taste FX)", "D_MIX"), ("GLOBAL (Taste GLO)", "D_GLO")]


def gen_params():
    out = []
    for title, d in TRACKS:
        defs = parse_defs(d)
        out.append(f"#### {title}\n")
        out.append("| Seite | Knob | Parameter | Bereich / Werte | Start |")
        out.append("|---|---|---|---|---|")
        for i, p in enumerate(defs):
            if p is None:
                continue
            page, knob = i // 4 + 1, i % 4 + 1
            if "alias" in p:
                out.append(f"| {page} | K{knob} | {p['name']} | wie auf der Spur-Seite | |")
            else:
                out.append(f"| {page} | K{knob} | {p['name']} | {p['range']} | {p['def']} |")
        out.append("")
    return "\n".join(out)


def gen_memes():
    names = re.findall(r'^\s*\{"([^"]+)", "([^"]+)", \d', MEMES, re.M)
    banks = re.search(r"BANKN\[6\] = \{([^}]*)\}", MEMES).group(1)
    banks = re.findall(r'"([^"]*)"', banks)
    assert len(names) == 96, len(names)
    keys = ["F3", "G3", "A3", "H3", "C4", "D4", "E4", "F4", "G4", "A4", "H4", "C5", "D5", "E5", "F5", "G5"]
    out = ["| Taste (unten) | " + " | ".join(f"Bank {chr(65 + b)} · {banks[b]} (OP{b + 1})" for b in range(6)) + " |",
           "|---|" + "---|" * 6]
    for i in range(16):
        row = [f"{i + 1} ({keys[i]})"] + [f"{names[b * 16 + i][0]} `{names[b * 16 + i][1]}`" for b in range(6)]
        out.append("| " + " | ".join(row) + " |")
    return "\n".join(out)


GEN = {"VERSION": lambda: f"**Version {VERSION}**", "PARAMS": gen_params, "MEMES": gen_memes}


def filled(text):
    for k, f in GEN.items():
        text = re.sub(rf"(<!-- GEN:{k} -->).*?(<!-- /GEN:{k} -->)", lambda m: m.group(1) + "\n" + f() + "\n" + m.group(2), text, flags=re.S)
    return text


def html(md_text):
    import markdown
    body = markdown.markdown(md_text, extensions=["tables", "toc", "fenced_code"], extension_configs={"toc": {"title": "INHALT.EXE"}})
    return HTML_HEAD.replace("@VERSION@", VERSION) + body + HTML_TAIL


HTML_HEAD = """<!DOCTYPE html>
<html lang="de"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>MEMETEK Handbuch v@VERSION@</title>
<link rel="preconnect" href="https://fonts.googleapis.com"><link rel="preconnect" href="https://fonts.gstatic.com" crossorigin>
<link href="https://fonts.googleapis.com/css2?family=Press+Start+2P&family=VT323&display=swap" rel="stylesheet">
<style>
:root{--bg:#0b0013;--ink:#f3f0ff;--pink:#ff2bd6;--cyan:#00f0ff;--lime:#b6ff00;--yel:#ffe600;--org:#ff7a00;--px:4px}
*{box-sizing:border-box}
html{background:#0b0013}
body{margin:0;color:var(--ink);font:22px/1.35 'VT323',ui-monospace,monospace;
 background:repeating-linear-gradient(0deg,rgba(255,255,255,.035) 0 2px,transparent 2px 4px),
 radial-gradient(circle at 20% 0%,#3a0066 0%,transparent 45%),radial-gradient(circle at 100% 30%,#003a4d 0%,transparent 40%),#0b0013}
body:after{content:"";position:fixed;inset:0;pointer-events:none;background:repeating-linear-gradient(0deg,rgba(0,0,0,.25) 0 1px,transparent 1px 3px);mix-blend-mode:multiply}
main{max-width:900px;margin:0 auto;padding:16px 16px 80px;position:relative}
.marquee{overflow:hidden;white-space:nowrap;background:var(--yel);color:#000;font:12px 'Press Start 2P',monospace;padding:8px 0;border-bottom:var(--px) solid #000}
.marquee span{display:inline-block;padding-left:100%;animation:mq 22s linear infinite}
@keyframes mq{to{transform:translateX(-100%)}}
h1,h2,h3,h4{font-family:'Press Start 2P',monospace;line-height:1.4;letter-spacing:.02em}
h1{font-size:30px;text-align:center;margin:28px 0 6px;color:#fff;
 text-shadow:3px 0 var(--pink),-3px 0 var(--cyan),0 6px 0 #000;animation:gl 3.5s steps(1) infinite}
@keyframes gl{0%,92%,100%{transform:none}93%{transform:translate(2px,-1px) skewX(4deg)}95%{transform:translate(-3px,1px)}97%{transform:skewX(-6deg)}}
h2{font-size:17px;color:#000;background:var(--lime);display:inline-block;padding:10px 12px;margin:46px 0 10px;
 box-shadow:var(--px) var(--px) 0 var(--pink),calc(var(--px)*2) calc(var(--px)*2) 0 #000;transform:rotate(-1deg)}
h2:nth-of-type(3n+2){background:var(--cyan);transform:rotate(.8deg)}
h2:nth-of-type(3n){background:var(--yel);transform:rotate(-.5deg)}
h3{font-size:13px;color:var(--pink);margin:26px 0 6px;text-shadow:2px 2px 0 #000}
h4{font-size:11px;color:var(--lime);margin:22px 0 4px}
h3:before{content:"\25B6 "}h4:before{content:"\2665 ";color:var(--pink)}
a{color:var(--cyan);text-decoration:none;border-bottom:2px dashed var(--cyan)}a:hover{background:var(--cyan);color:#000}
strong{color:var(--yel)}
code{font-family:'VT323',monospace;background:#000;color:var(--lime);padding:0 6px;border:2px solid var(--lime)}
pre{background:#000;border:var(--px) solid var(--cyan);box-shadow:var(--px) var(--px) 0 var(--pink);padding:12px;overflow-x:auto;font-size:18px;line-height:1.2}
pre code{border:0;padding:0;color:var(--cyan)}
table{border-collapse:separate;border-spacing:0;width:100%;display:block;overflow-x:auto;margin:12px 0 18px;
 border:var(--px) solid #fff;box-shadow:var(--px) var(--px) 0 var(--pink),calc(var(--px)*2) calc(var(--px)*2) 0 var(--cyan);background:#120020;font-size:19px}
th{font:10px/1.5 'Press Start 2P',monospace;background:var(--pink);color:#000;text-align:left;padding:8px}
td{padding:5px 8px;border-top:2px dotted #4a2a6a;vertical-align:top}
tr:nth-child(even) td{background:#1b0630}
tr:hover td{background:#2b0b4a}
blockquote{margin:18px 0;padding:10px 14px;background:#000;border:var(--px) dashed var(--yel);color:var(--yel)}
blockquote p{margin:4px 0}
ul,ol{padding-left:26px}li{margin:4px 0}li::marker{color:var(--pink);content:"\25A0  "}
ol li::marker{content:counter(list-item) ". ";font-family:'Press Start 2P';font-size:11px;color:var(--lime)}
.toc{background:#000;border:var(--px) solid var(--lime);padding:6px 16px 12px;box-shadow:var(--px) var(--px) 0 var(--cyan);margin:18px 0}
.toc .toctitle{font:12px 'Press Start 2P';color:var(--lime);display:block;margin:10px 0}
.toc ul{list-style:none;padding-left:14px}.toc li::marker{content:""}.toc a{border:0}
.sticker{position:absolute;font:11px 'Press Start 2P',monospace;padding:8px 10px;color:#000;z-index:2;
 box-shadow:3px 3px 0 #000;pointer-events:none;white-space:nowrap}
.hero{text-align:center;margin:8px 0 20px}
.hero .sub{font:11px 'Press Start 2P';color:var(--cyan);margin-top:6px}
.hero .meme{font:900 28px Impact,'Arial Black',sans-serif;color:#fff;text-transform:uppercase;letter-spacing:1px;
 -webkit-text-stroke:2px #000;text-shadow:3px 3px 0 #000;margin:12px 0 0}
.blink{animation:bl 1s steps(1) infinite}@keyframes bl{50%{opacity:0}}
.pixbar{height:16px;margin:18px 0;background:repeating-linear-gradient(90deg,var(--pink) 0 16px,var(--cyan) 16px 32px,var(--lime) 32px 48px,var(--yel) 48px 64px,var(--org) 64px 80px)}
footer{text-align:center;font:10px 'Press Start 2P';color:#8a6aa8;margin-top:40px}
@media (max-width:600px){body{font-size:20px}h1{font-size:20px}h2{font-size:13px}.sticker{display:none}}
@media (prefers-reduced-motion:reduce){*{animation:none!important}}
</style></head><body>
<div class="marquee"><span>&#9733; MEMETEK v@VERSION@ &#9733; 96 MEMES &#9733; TEK KICK GO BRRR &#9733; 8-BIT OR DIE &#9733; NO CAP &#9733; STONKS &#9733; BRUH &#9733; SHEESH &#9733; YEET THE BASS &#9733;</span></div>
<main>
<span class="sticker" style="top:120px;right:-30px;background:var(--yel);transform:rotate(9deg)">NO CAP</span>
<span class="sticker" style="top:640px;left:-46px;background:var(--cyan);transform:rotate(-12deg)">STONKS &#8599;</span>
<span class="sticker" style="top:1500px;right:-40px;background:var(--pink);transform:rotate(6deg)">BRUH.</span>
<span class="sticker" style="top:2600px;left:-40px;background:var(--lime);transform:rotate(-7deg)">SHEESH</span>
<span class="sticker" style="top:3800px;right:-34px;background:var(--org);transform:rotate(11deg)">GG EZ</span>
<div class="hero"><div class="meme">wenn die kick 8-bit ist</div><div class="sub">&#9608; HANDBUCH.TXT &#9608; v@VERSION@ <span class="blink">_</span></div></div>
<div class="pixbar"></div>
"""
HTML_TAIL = """
<div class="pixbar"></div>
<footer>MEMETEK &middot; GPL-3.0 &middot; MADE WITH 100% REAL FAKE SAMPLES &middot; <span class="blink">PRESS START</span></footer>
</main></body></html>"""


def main(argv):
    text = DOC.read_text()
    new = filled(text)
    if "--check" in argv:
        if new != text:
            print("docs/HANDBUCH.md: the tables are behind the source; run tools/handbuch.py")
            return 1
        print("docs/HANDBUCH.md: up to date")
        return 0
    if new != text:
        DOC.write_text(new)
        print("docs/HANDBUCH.md: tables updated")
    if "--html" in argv:
        out = Path(argv[argv.index("--html") + 1])
        out.parent.mkdir(parents=True, exist_ok=True)
        out.write_text(html(new))
        print("->", out)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
