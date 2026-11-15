/* SPDX-License-Identifier: GPL-3.0-only */
/* Memetek FM1's 96 meme sounds, six banks of 16, synthesised live: each sound is up to four layers, each layer an
 * oscillator (or noise) with a pitch path or a note sequence, envelopes, vibrato, amplitude modulation, a
 * filter with its own path and LFO, repeats. Made after the browser prototype's Web Audio patches. */

typedef struct {
    float delay;                /* the layer starts this late (s) */
    uint8_t wave;               /* 0 square 1 pulse25 2 saw 3 tri 4 pulse12 5 sine 9 none (noise only) */
    uint8_t nosc;               /* 1..4 oscillators: the others at ratio[] */
    float ratio[3];
    float f[4], t[3];           /* pitch: f0, exp ramps to f1 in t0, f2 in t1, f3 in t2 (t 0: stays) */
    const float *seq;           /* a note sequence in place of the pitch path */
    uint8_t nseq, count, seqrand;   /* the pool, the notes played (0: nseq), random picks */
    float seql, lastl, gate;    /* note length, the last one's (0: seql), the part of it sounding (0: .8) */
    float att, hold, rel, amp;  /* linear attack, hold, exponential release (the sequence: over all of it) */
    float vibf, vibd, vibdec;   /* vibrato rate, depth (a fraction of f), its fall time (0: steady) */
    float amf, amd;             /* amplitude modulation rate, depth */
    uint8_t amsq;               /* 1: square */
    float noise, noisedec;      /* a noise layer: level, decay (0: steady) */
    float nr0, nr1;             /* sample-and-hold noise, its rate as a fraction of 44.1 kHz, ramped (0: white) */
    float crackle;              /* random clicks per second gate the noise */
    float jitf, jitd;           /* random pitch steps: rate, depth (fraction) */
    uint8_t ftype;              /* 0 none 1 low 2 band 3 high */
    float fc0, fc1, fc2, fcm, q;/* cutoff fc0 -> fc1 at fcm (part of the length, 0: .5) -> fc2; q (LP/HP in dB, 0: .707) */
    float flf, fld;             /* filter LFO: rate, depth in octaves */
    uint8_t reps;               /* plays (0, 1: once) */
    float gap, repdec;          /* the period of a repeat (0: its length), the level of each next (0: 1) */
    float drive;
} layer_t;

typedef struct { const char *name, *abbr; uint8_t nl; layer_t L[4]; } meme_t;

static const char *const BANKN[6] = {"CARTOON", "ARCADE", "MEME", "ANIMAL", "SCI-FI", "PARTY"};

static const float S_TIPTOE[] = {784, 659, 784, 659};
static const float S_BONGO[] = {350, 462};
static const float S_BALLOON[] = {900, 1300, 800, 1500, 700, 1200, 600};
static const float S_COIN[] = {988, 1319};
static const float S_1UP[] = {523, 659, 784, 1047, 1319};
static const float S_POWER[] = {220, 248.6f, 280.9f, 317.4f, 358.7f, 405.3f, 458, 517.6f, 584.9f, 660.9f, 746.8f, 843.9f, 953.6f, 1077.6f};
static const float S_GAMEOVR[] = {523, 415, 349, 262};
static const float S_PIPE[] = {880, 740, 622, 523, 440, 370, 311, 262, 220};
static const float S_SELECT[] = {1568, 2093};
static const float S_VICTORY[] = {392, 523, 659, 784, 659, 784, 1047};
static const float S_SAD[] = {311, 294, 277, 262};
static const float S_CUCKOO[] = {784, 622};
static const float S_KAZOO[] = {392, 440, 523, 392};
static const float S_ROOSTER[] = {530, 590, 630, 600};
static const float S_ROBOT[] = {820, 610, 910, 520, 1040, 700};
static const float S_TELE[] = {440, 494, 554, 659, 740, 880, 988, 1109, 1319, 1480, 1760, 1976, 2217, 2637, 2960, 3520, 3951, 4434};
static const float S_GLITCH[] = {300, 700, 1200, 1800, 2400, 3000, 450, 950};
static const float S_MODEM[] = {1200, 2100, 980, 1650, 2400, 1300};
static const float S_DATA[] = {800, 1020, 1240, 1460, 1680, 1900, 2120, 2340};
static const float S_LAUGH[] = {300, 282, 264, 246};
static const float S_SPARK[] = {2100, 2600, 3100, 3700, 4300, 4800};

#define SQ 0
#define PU 1
#define SAW 2
#define TRI 3
#define SIN 5
#define NO 9
#define LP 1
#define BP 2
#define HP 3
#define SEQ(a) .seq = a, .nseq = (uint8_t)(sizeof a / sizeof a[0])
#define HORN_L(d, h) {.delay = d, .wave = SAW, .nosc = 4, .ratio = {470.0f / 466, 698.0f / 466, 703.0f / 466}, .f = {466}, \
                      .att = 0.015f, .hold = h, .rel = 0.08f, .amp = 0.35f, .ftype = BP, .fc0 = 1300, .q = 0.8f}

static const meme_t MEMES[96] = {
    /* ---- A CARTOON */
    {"BOING", "BNG", 1, {{.wave = SIN, .f = {300, 700, 140}, .t = {0.07f, 0.48f}, .att = 0.005f, .rel = 0.55f, .amp = 0.7f, .vibf = 16, .vibd = 0.25f, .vibdec = 0.5f}}},
    {"SPRING", "SPR", 1, {{.wave = TRI, .f = {170, 260}, .t = {1.1f}, .att = 0.003f, .rel = 1.1f, .amp = 0.7f, .vibf = 24, .vibd = 0.7f, .vibdec = 1}}},
    {"SLIDE UP", "SL^", 1, {{.wave = SIN, .f = {380, 1700}, .t = {0.5f}, .att = 0.04f, .hold = 0.41f, .rel = 0.1f, .amp = 0.45f, .vibf = 7, .vibd = 0.02f}}},
    {"SLIDE DN", "SLv", 1, {{.wave = SIN, .f = {1700, 330}, .t = {0.6f}, .att = 0.04f, .hold = 0.51f, .rel = 0.1f, .amp = 0.45f, .vibf = 7, .vibd = 0.02f}}},
    {"BONK", "BNK", 2, {{.wave = SIN, .f = {650, 140}, .t = {0.09f}, .att = 0.001f, .rel = 0.18f, .amp = 1},
                         {.wave = NO, .noise = 1, .att = 0.001f, .rel = 0.04f, .amp = 0.5f, .ftype = BP, .fc0 = 1800, .q = 2}}},
    {"POP", "POP", 2, {{.wave = SIN, .f = {1400, 250}, .t = {0.04f}, .att = 0.001f, .rel = 0.06f, .amp = 0.9f},
                        {.wave = NO, .noise = 1, .att = 0.001f, .rel = 0.02f, .amp = 0.4f, .ftype = HP, .fc0 = 3000}}},
    {"WHIP", "WHP", 2, {{.wave = NO, .noise = 1, .att = 0.06f, .rel = 0.08f, .amp = 1.5f, .ftype = BP, .fc0 = 700, .fc1 = 7000, .fcm = 0.9f, .q = 4},
                         {.delay = 0.12f, .wave = NO, .noise = 1, .att = 0.001f, .rel = 0.03f, .amp = 1, .ftype = HP, .fc0 = 4000}}},
    {"ZIP", "ZIP", 1, {{.wave = NO, .noise = 1, .att = 0.02f, .hold = 0.28f, .rel = 0.04f, .amp = 1, .amf = 55, .amd = 1, .ftype = BP, .fc0 = 600, .fc1 = 4500, .fcm = 0.95f, .q = 3}}},
    {"TWANG", "TWG", 1, {{.wave = SAW, .f = {110, 70}, .t = {0.7f}, .att = 0.002f, .rel = 0.7f, .amp = 0.6f, .vibf = 14, .vibd = 0.36f, .vibdec = 0.7f, .ftype = LP, .fc0 = 1500, .q = 4}}},
    {"SQUEAK", "SQK", 1, {{.wave = SIN, .f = {1700, 2500, 2100}, .t = {0.12f, 0.08f}, .att = 0.01f, .hold = 0.18f, .rel = 0.04f, .amp = 0.35f, .vibf = 45, .vibd = 0.045f}}},
    {"TIPTOE", "TIP", 1, {{.wave = TRI, SEQ(S_TIPTOE), .seql = 0.11f, .att = 0.002f, .amp = 0.5f}}},
    {"PAN", "PAN", 4, {{.wave = SIN, .f = {523}, .att = 0.001f, .rel = 0.9f, .amp = 0.5f},
                        {.wave = SIN, .f = {1311}, .att = 0.001f, .rel = 0.75f, .amp = 0.3f},
                        {.wave = SIN, .f = {2120}, .att = 0.001f, .rel = 0.6f, .amp = 0.2f},
                        {.wave = NO, .noise = 1, .att = 0.001f, .rel = 0.03f, .amp = 0.6f, .ftype = BP, .fc0 = 3000}}},
    {"BONGO", "BGO", 1, {{.wave = SIN, SEQ(S_BONGO), .seql = 0.055f, .gate = 0.6f, .att = 0.001f, .amp = 0.6f, .reps = 5}}},
    {"BALLOON", "BLN", 1, {{.wave = SAW, SEQ(S_BALLOON), .seql = 0.11f, .gate = 1, .att = 0.03f, .amp = 1.5f, .vibf = 23, .vibd = 0.06f, .ftype = BP, .fc0 = 1500, .q = 6}}},
    {"SPLAT", "SPL", 2, {{.wave = NO, .noise = 1, .att = 0.002f, .rel = 0.25f, .amp = 1.2f, .ftype = LP, .fc0 = 1800},
                          {.wave = SIN, .f = {300, 60}, .t = {0.1f}, .att = 0.002f, .rel = 0.12f, .amp = 0.7f}}},
    {"HICCUP", "HIC", 2, {{.wave = SAW, .f = {320, 520}, .t = {0.05f}, .att = 0.003f, .rel = 0.08f, .amp = 3, .ftype = BP, .fc0 = 600, .q = 4},
                           {.wave = NO, .noise = 1, .att = 0.001f, .rel = 0.02f, .amp = 0.4f, .ftype = BP, .fc0 = 2500, .q = 2}}},
    /* ---- B ARCADE */
    {"PEW", "PEW", 1, {{.wave = SQ, .f = {1700, 160}, .t = {0.2f}, .att = 0.002f, .rel = 0.2f, .amp = 0.5f}}},
    {"COIN", "CON", 1, {{.wave = SQ, SEQ(S_COIN), .seql = 0.07f, .lastl = 0.35f, .gate = 1, .att = 0.002f, .rel = 0.4f, .amp = 0.45f}}},
    {"JUMP", "JMP", 1, {{.wave = SQ, .f = {280, 950}, .t = {0.16f}, .att = 0.002f, .rel = 0.2f, .amp = 0.4f}}},
    {"1UP", "1UP", 1, {{.wave = SQ, SEQ(S_1UP), .seql = 0.07f, .gate = 0.9f, .att = 0.001f, .amp = 0.35f}}},
    {"POWER", "PWR", 1, {{.wave = SQ, SEQ(S_POWER), .seql = 0.035f, .gate = 0.9f, .att = 0.001f, .amp = 0.3f}}},
    {"HIT", "HIT", 2, {{.wave = SQ, .f = {180, 50}, .t = {0.2f}, .att = 0.001f, .rel = 0.22f, .amp = 0.6f},
                        {.wave = NO, .noise = 1, .att = 0.001f, .rel = 0.15f, .amp = 0.6f, .ftype = LP, .fc0 = 1500}}},
    {"LASER", "LSR", 1, {{.wave = SAW, .f = {2800, 90}, .t = {0.35f}, .att = 0.002f, .rel = 0.35f, .amp = 0.4f, .vibf = 40, .vibd = 0.07f}}},
    {"GAMEOVR", "G.O", 1, {{.wave = TRI, SEQ(S_GAMEOVR), .seql = 0.2f, .lastl = 0.6f, .gate = 0.9f, .att = 0.002f, .amp = 0.6f}}},
    {"PIPE", "PIP", 1, {{.wave = SQ, SEQ(S_PIPE), .seql = 0.045f, .gate = 0.9f, .att = 0.001f, .amp = 0.3f}}},
    {"SELECT", "SEL", 1, {{.wave = SQ, SEQ(S_SELECT), .seql = 0.03f, .lastl = 0.05f, .gate = 1, .att = 0.001f, .rel = 0.07f, .amp = 0.35f}}},
    {"BOMB", "BMB", 1, {{.wave = NO, .noise = 1, .nr0 = 0.25f, .nr1 = 0.03f, .att = 0.002f, .rel = 0.9f, .amp = 0.9f, .ftype = LP, .fc0 = 3000}}},
    {"PUNCH", "PCH", 2, {{.wave = SQ, .f = {220, 55}, .t = {0.1f}, .att = 0.001f, .rel = 0.12f, .amp = 0.9f},
                          {.wave = NO, .noise = 1, .att = 0.001f, .rel = 0.08f, .amp = 0.7f, .ftype = LP, .fc0 = 2500}}},
    {"CHARGE", "CHG", 1, {{.wave = SQ, .f = {200, 1400}, .t = {0.9f}, .att = 0.01f, .hold = 0.9f, .rel = 0.05f, .amp = 0.3f, .vibf = 12, .vibd = 0.05f}}},
    {"VICTORY", "VIC", 1, {{.wave = SQ, SEQ(S_VICTORY), .seql = 0.09f, .lastl = 0.45f, .gate = 0.9f, .att = 0.001f, .amp = 0.32f}}},
    {"DEATH", "DTH", 1, {{.wave = SQ, .f = {800, 100}, .t = {0.8f}, .att = 0.005f, .hold = 0.8f, .rel = 0.05f, .amp = 0.35f, .vibf = 18, .vibd = 0.25f}}},
    {"BUMP", "BMP", 1, {{.wave = SQ, .f = {140, 70}, .t = {0.08f}, .att = 0.001f, .rel = 0.09f, .amp = 0.5f}}},
    /* ---- C MEME */
    {"HORN", "HRN", 1, {HORN_L(0, 0.3f)}},
    {"BOOM", "BOM", 2, {{.wave = SIN, .f = {120, 30}, .t = {0.5f}, .att = 0.002f, .rel = 1.5f, .amp = 1, .drive = 3},
                         {.wave = NO, .noise = 1, .att = 0.002f, .rel = 0.25f, .amp = 0.8f, .ftype = LP, .fc0 = 300, .drive = 3}}},
    {"FART", "FRT", 1, {{.wave = SAW, .f = {85}, .jitf = 50, .jitd = 0.35f, .att = 0.02f, .rel = 0.45f, .amp = 0.9f, .noise = 0.3f, .noisedec = 0.4f, .ftype = LP, .fc0 = 450, .q = 7}}},
    {"SAD", "SAD", 1, {{.wave = SAW, SEQ(S_SAD), .seql = 0.32f, .lastl = 1.0f, .gate = 0.85f, .att = 0.04f, .amp = 0.6f, .vibf = 6, .vibd = 0.03f, .ftype = LP, .fc0 = 1000, .q = 2}}},
    {"RIMSHOT", "RIM", 3, {{.wave = SIN, .f = {200, 100}, .t = {0.15f}, .att = 0.001f, .rel = 0.18f, .amp = 0.9f},
                            {.delay = 0.16f, .wave = SIN, .f = {150, 75}, .t = {0.15f}, .att = 0.001f, .rel = 0.18f, .amp = 0.9f},
                            {.delay = 0.34f, .wave = NO, .noise = 1, .att = 0.001f, .rel = 0.7f, .amp = 0.5f, .ftype = HP, .fc0 = 6500}}},
    {"WOLF", "WLF", 2, {{.wave = SIN, .f = {900, 2300}, .t = {0.22f}, .att = 0.03f, .hold = 0.18f, .rel = 0.05f, .amp = 0.4f},
                         {.delay = 0.35f, .wave = SIN, .f = {900, 2400, 650}, .t = {0.2f, 0.35f}, .att = 0.03f, .hold = 0.5f, .rel = 0.07f, .amp = 0.4f}}},
    {"CRICKET", "CRK", 1, {{.wave = SIN, .f = {4400}, .att = 0.002f, .hold = 0.2f, .rel = 0.01f, .amp = 0.3f, .amf = 28, .amd = 1, .amsq = 1, .reps = 2, .gap = 0.5f}}},
    {"BRUH", "BRH", 2, {{.wave = SAW, .f = {125, 88}, .t = {0.45f}, .att = 0.015f, .rel = 0.45f, .amp = 2.5f, .ftype = BP, .fc0 = 350, .q = 4},
                         {.wave = SAW, .f = {125, 88}, .t = {0.45f}, .att = 0.015f, .rel = 0.45f, .amp = 1.5f, .ftype = BP, .fc0 = 900, .q = 6}}},
    {"OOF", "OOF", 2, {{.wave = SAW, .f = {150, 100}, .t = {0.24f}, .att = 0.01f, .rel = 0.22f, .amp = 3, .ftype = BP, .fc0 = 420, .q = 5},
                        {.wave = SAW, .f = {150, 100}, .t = {0.24f}, .att = 0.01f, .rel = 0.22f, .amp = 2, .ftype = BP, .fc0 = 800, .q = 6}}},
    {"DUNDUN", "DUN", 3, {{.wave = SAW, .nosc = 3, .ratio = {0.5f, 1.5f}, .f = {98}, .att = 0.01f, .hold = 0.25f, .rel = 0.15f, .amp = 0.4f, .ftype = LP, .fc0 = 900, .q = 2},
                           {.delay = 0.3f, .wave = SAW, .nosc = 3, .ratio = {0.5f, 1.5f}, .f = {98}, .att = 0.01f, .hold = 0.25f, .rel = 0.15f, .amp = 0.4f, .ftype = LP, .fc0 = 900, .q = 2},
                           {.delay = 0.6f, .wave = SAW, .nosc = 3, .ratio = {0.5f, 1.5f}, .f = {92}, .att = 0.01f, .hold = 0.9f, .rel = 0.15f, .amp = 0.4f, .ftype = LP, .fc0 = 900, .q = 2}}},
    {"BOI", "BOI", 2, {{.wave = SAW, .f = {120, 260}, .t = {0.3f}, .att = 0.02f, .hold = 0.3f, .rel = 0.08f, .amp = 2.5f, .ftype = BP, .fc0 = 500, .q = 4},
                        {.wave = SAW, .f = {120, 260}, .t = {0.3f}, .att = 0.02f, .hold = 0.3f, .rel = 0.08f, .amp = 1.5f, .ftype = BP, .fc0 = 1200, .q = 6}}},
    {"TAPESTP", "TPS", 1, {{.wave = SAW, .nosc = 3, .ratio = {330.0f / 262, 392.0f / 262}, .f = {262, 262, 16}, .t = {0.1f, 0.75f}, .att = 0.01f, .hold = 0.8f, .rel = 0.1f, .amp = 0.3f,
                             .ftype = LP, .fc0 = 2000, .fc1 = 2000, .fc2 = 200, .fcm = 0.12f}}},
    {"EARRAPE", "EAR", 1, {{.wave = SQ, .nosc = 4, .ratio = {277.0f / 220, 330.0f / 220, 2}, .f = {220}, .att = 0.005f, .hold = 0.45f, .rel = 0.05f, .amp = 0.5f, .noise = 0.5f, .drive = 40}}},
    {"WOW", "WOW", 1, {{.wave = SAW, .f = {170, 140}, .t = {0.6f}, .att = 0.04f, .hold = 0.5f, .rel = 0.15f, .amp = 3, .ftype = BP, .fc0 = 350, .fc1 = 1300, .fc2 = 450, .fcm = 0.4f, .q = 5}}},
    {"VIBRATE", "VIB", 1, {{.wave = SAW, .f = {150}, .att = 0.01f, .hold = 0.3f, .rel = 0.02f, .amp = 1, .amf = 25, .amd = 1, .ftype = LP, .fc0 = 400, .q = 2, .reps = 2, .gap = 0.45f}}},
    {"DRAMATIC", "DRM", 3, {{.wave = SAW, .nosc = 4, .ratio = {196.0f / 131, 262.0f / 131, 311.0f / 131}, .f = {131}, .att = 0.003f, .rel = 1, .amp = 0.5f, .ftype = LP, .fc0 = 3000},
                             {.wave = SAW, .nosc = 2, .ratio = {523.0f / 392}, .f = {392}, .att = 0.003f, .rel = 1, .amp = 0.35f, .ftype = LP, .fc0 = 3000},
                             {.wave = NO, .noise = 1, .att = 0.001f, .rel = 0.15f, .amp = 0.6f, .ftype = LP, .fc0 = 1500}}},
    /* ---- D ANIMAL */
    {"QUACK", "QAK", 1, {{.wave = SAW, .f = {420, 300}, .t = {0.17f}, .att = 0.005f, .rel = 0.17f, .amp = 2, .ftype = BP, .fc0 = 1600, .fc1 = 800, .fcm = 1, .q = 3}}},
    {"MEOW", "MEW", 1, {{.wave = SAW, .f = {480, 780, 430}, .t = {0.25f, 0.37f}, .att = 0.08f, .hold = 0.45f, .rel = 0.2f, .amp = 2,
                          .ftype = BP, .fc0 = 500, .fc1 = 1900, .fc2 = 650, .fcm = 0.4f, .q = 4}}},
    {"CUCKOO", "CUC", 1, {{.wave = SIN, SEQ(S_CUCKOO), .seql = 0.28f, .gate = 0.9f, .att = 0.01f, .amp = 0.55f}}},
    {"HONK", "HNK", 1, {{.wave = SQ, .nosc = 2, .ratio = {336.0f / 330}, .f = {330}, .att = 0.01f, .hold = 0.12f, .rel = 0.06f, .amp = 0.5f, .ftype = LP, .fc0 = 1600, .q = 3, .reps = 2, .gap = 0.24f}}},
    {"KAZOO", "KAZ", 1, {{.wave = SAW, SEQ(S_KAZOO), .seql = 0.14f, .lastl = 0.3f, .gate = 0.9f, .att = 0.01f, .amp = 0.9f, .vibf = 6, .vibd = 0.025f, .noise = 0.1f, .ftype = BP, .fc0 = 1100, .q = 2}}},
    {"SIREN", "SIR", 1, {{.wave = SQ, .f = {750}, .att = 0.02f, .hold = 0.75f, .rel = 0.1f, .amp = 0.35f, .vibf = 3.5f, .vibd = 0.37f}}},
    {"BURP", "BRP", 1, {{.wave = SAW, .f = {75, 60}, .t = {0.5f}, .jitf = 30, .jitd = 0.2f, .att = 0.03f, .rel = 0.5f, .amp = 2.5f, .amf = 28, .amd = 0.8f, .ftype = BP, .fc0 = 450, .q = 5}}},
    {"BLUB", "BLB", 3, {{.wave = SIN, .f = {250, 1100}, .t = {0.06f}, .att = 0.002f, .rel = 0.07f, .amp = 0.5f},
                         {.delay = 0.11f, .wave = SIN, .f = {340, 1300}, .t = {0.06f}, .att = 0.002f, .rel = 0.07f, .amp = 0.5f},
                         {.delay = 0.2f, .wave = SIN, .f = {430, 1500}, .t = {0.06f}, .att = 0.002f, .rel = 0.07f, .amp = 0.5f}}},
    {"CHIRP", "CHP", 1, {{.wave = SIN, .f = {4200, 2600}, .t = {0.07f}, .att = 0.003f, .rel = 0.07f, .amp = 0.35f, .reps = 3, .gap = 0.12f}}},
    {"FROG", "FRG", 1, {{.wave = SQ, .f = {110}, .att = 0.005f, .hold = 0.1f, .rel = 0.03f, .amp = 1, .amf = 45, .amd = 0.9f, .ftype = LP, .fc0 = 900, .q = 4, .reps = 2, .gap = 0.18f}}},
    {"DOG", "DOG", 2, {{.wave = SAW, .f = {380, 260}, .t = {0.12f}, .att = 0.005f, .rel = 0.13f, .amp = 3, .noise = 0.3f, .noisedec = 0.06f, .ftype = BP, .fc0 = 700, .q = 3, .reps = 2, .gap = 0.25f},
                        {.wave = SAW, .f = {380, 260}, .t = {0.12f}, .att = 0.005f, .rel = 0.13f, .amp = 2, .ftype = BP, .fc0 = 1500, .q = 4, .reps = 2, .gap = 0.25f}}},
    {"COW", "COW", 1, {{.wave = SAW, .f = {130, 110}, .t = {1.2f}, .att = 0.1f, .hold = 0.9f, .rel = 0.3f, .amp = 3, .ftype = BP, .fc0 = 250, .fc1 = 900, .fc2 = 500, .fcm = 0.33f, .q = 4}}},
    {"BEE", "BEE", 1, {{.wave = SAW, .f = {210}, .jitf = 8, .jitd = 0.1f, .att = 0.05f, .hold = 0.9f, .rel = 0.15f, .amp = 1.2f, .amf = 30, .amd = 0.4f, .ftype = BP, .fc0 = 900, .q = 2}}},
    {"OWL", "OWL", 2, {{.wave = SIN, .f = {400, 360}, .t = {0.3f}, .att = 0.08f, .hold = 0.3f, .rel = 0.15f, .amp = 0.4f},
                        {.delay = 0.45f, .wave = SIN, .f = {400, 360}, .t = {0.5f}, .att = 0.08f, .hold = 0.5f, .rel = 0.15f, .amp = 0.4f}}},
    {"GOAT", "GOT", 2, {{.wave = SAW, .f = {300, 260}, .t = {0.6f}, .att = 0.03f, .hold = 0.55f, .rel = 0.15f, .amp = 2.5f, .vibf = 11, .vibd = 0.1f, .ftype = BP, .fc0 = 700, .q = 4},
                         {.wave = SAW, .f = {300, 260}, .t = {0.6f}, .att = 0.03f, .hold = 0.55f, .rel = 0.15f, .amp = 1.5f, .vibf = 11, .vibd = 0.1f, .ftype = BP, .fc0 = 1300, .q = 5}}},
    {"ROOSTER", "ROO", 2, {{.wave = SAW, SEQ(S_ROOSTER), .seql = 0.15f, .lastl = 0.7f, .gate = 0.85f, .att = 0.01f, .amp = 2.4f, .ftype = BP, .fc0 = 800, .q = 4},
                            {.wave = SAW, SEQ(S_ROOSTER), .seql = 0.15f, .lastl = 0.7f, .gate = 0.85f, .att = 0.01f, .amp = 1.5f, .ftype = BP, .fc0 = 1600, .q = 5}}},
    /* ---- E SCI-FI */
    {"ROBOT", "ROB", 1, {{.wave = SQ, SEQ(S_ROBOT), .seql = 0.065f, .gate = 0.9f, .att = 0.001f, .amp = 0.9f, .ftype = BP, .fc0 = 1500, .q = 1}}},
    {"BEEP", "BEP", 1, {{.wave = SIN, .f = {1000}, .att = 0.005f, .hold = 0.42f, .rel = 0.02f, .amp = 0.4f}}},
    {"UFO", "UFO", 1, {{.wave = SIN, .f = {420, 950}, .t = {1.2f}, .att = 0.05f, .hold = 1.1f, .rel = 0.15f, .amp = 0.4f, .vibf = 9, .vibd = 0.2f}}},
    {"WARP", "WRP", 1, {{.wave = SAW, .f = {90, 2200}, .t = {0.6f}, .att = 0.02f, .hold = 0.5f, .rel = 0.12f, .amp = 1.2f, .vibf = 30, .vibd = 0.05f, .ftype = BP, .fc0 = 400, .fc1 = 5000, .fcm = 1, .q = 3}}},
    {"ERROR", "ERR", 1, {{.wave = SQ, .nosc = 2, .ratio = {233.0f / 220}, .f = {220}, .att = 0.005f, .hold = 0.14f, .rel = 0.02f, .amp = 0.5f, .ftype = LP, .fc0 = 1800, .reps = 2, .gap = 0.22f}}},
    {"NOTIF", "NTF", 4, {{.wave = SIN, .f = {1319}, .att = 0.002f, .rel = 0.55f, .amp = 0.45f},
                          {.delay = 0.12f, .wave = SIN, .f = {1760}, .att = 0.002f, .rel = 0.55f, .amp = 0.45f},
                          {.wave = SIN, .f = {3640}, .att = 0.002f, .rel = 0.2f, .amp = 0.12f},
                          {.delay = 0.12f, .wave = SIN, .f = {4858}, .att = 0.002f, .rel = 0.2f, .amp = 0.12f}}},
    {"TELEPRT", "TLP", 1, {{.wave = SIN, SEQ(S_TELE), .seql = 0.028f, .lastl = 0.15f, .gate = 0.9f, .att = 0.001f, .amp = 0.35f}}},
    {"GLITCH", "GLT", 1, {{.wave = SQ, SEQ(S_GLITCH), .count = 10, .seqrand = 1, .seql = 0.035f, .gate = 0.6f, .att = 0.001f, .amp = 0.35f, .noise = 0.3f}}},
    {"MODEM", "MDM", 2, {{.wave = SQ, SEQ(S_MODEM), .count = 14, .seqrand = 1, .seql = 0.06f, .gate = 0.85f, .att = 0.002f, .amp = 0.25f},
                          {.delay = 0.85f, .wave = NO, .noise = 1, .att = 0.01f, .hold = 0.4f, .rel = 0.05f, .amp = 0.4f, .ftype = BP, .fc0 = 2000}}},
    {"SONAR", "SON", 1, {{.wave = SIN, .f = {1250}, .att = 0.002f, .rel = 0.6f, .amp = 0.45f, .reps = 3, .gap = 0.35f, .repdec = 0.5f}}},
    {"ALARM", "ALM", 1, {{.wave = SAW, .f = {400, 900}, .t = {0.5f}, .att = 0.01f, .hold = 0.5f, .rel = 0.03f, .amp = 0.3f, .ftype = LP, .fc0 = 2500, .reps = 2, .gap = 0.55f}}},
    {"POWERDN", "PDN", 1, {{.wave = SAW, .f = {800, 25}, .t = {1.15f}, .att = 0.01f, .hold = 1.1f, .rel = 0.1f, .amp = 0.45f, .ftype = LP, .fc0 = 3000, .fc1 = 150, .fcm = 0.95f, .q = 3}}},
    {"BLASTER", "BLS", 2, {{.wave = SQ, .f = {2400, 300, 120}, .t = {0.12f, 0.13f}, .att = 0.001f, .rel = 0.25f, .amp = 0.5f},
                            {.wave = NO, .noise = 1, .att = 0.001f, .rel = 0.1f, .amp = 0.4f, .ftype = HP, .fc0 = 2000}}},
    {"DATA", "DAT", 1, {{.wave = SIN, SEQ(S_DATA), .count = 16, .seqrand = 1, .seql = 0.03f, .gate = 0.9f, .att = 0.001f, .amp = 0.3f}}},
    {"BEAM", "BEM", 1, {{.wave = SAW, .nosc = 2, .ratio = {223.0f / 220}, .f = {220}, .att = 0.1f, .hold = 1.1f, .rel = 0.2f, .amp = 1, .vibf = 7, .vibd = 0.18f,
                          .amf = 16, .amd = 0.5f, .ftype = BP, .fc0 = 900, .q = 3}}},
    {"HYPER", "HYP", 2, {{.wave = NO, .noise = 1, .att = 0.3f, .hold = 0.6f, .rel = 0.5f, .amp = 1.4f, .ftype = BP, .fc0 = 200, .fc1 = 6000, .fc2 = 800, .fcm = 0.62f, .q = 2},
                          {.wave = SIN, .f = {80, 1200}, .t = {0.9f}, .att = 0.3f, .hold = 0.6f, .rel = 0.5f, .amp = 0.3f}}},
    /* ---- F PARTY */
    {"HORN x3", "HR3", 2, {{.wave = SAW, .nosc = 4, .ratio = {470.0f / 466, 698.0f / 466, 703.0f / 466}, .f = {466}, .att = 0.015f, .hold = 0.09f, .rel = 0.05f, .amp = 0.35f,
                             .ftype = BP, .fc0 = 1300, .q = 0.8f, .reps = 2, .gap = 0.16f}, HORN_L(0.32f, 0.55f)}},
    {"SCRATCH", "SCR", 2, {{.wave = NO, .noise = 1, .att = 0.01f, .hold = 0.45f, .rel = 0.05f, .amp = 1.6f, .amf = 14, .amd = 0.8f, .ftype = BP, .fc0 = 1200, .q = 2, .flf = 12, .fld = 1.5f},
                            {.wave = SAW, .f = {180}, .att = 0.01f, .hold = 0.45f, .rel = 0.05f, .amp = 0.5f, .vibf = 12, .vibd = 0.6f, .ftype = BP, .fc0 = 1000, .q = 2}}},
    {"RISER", "RSR", 1, {{.wave = NO, .noise = 1, .att = 1.8f, .rel = 0.06f, .amp = 1.4f, .ftype = BP, .fc0 = 300, .fc1 = 9000, .fcm = 0.97f, .q = 2}}},
    {"DROP", "DRP", 1, {{.wave = SIN, .f = {170, 28}, .t = {1.2f}, .att = 0.01f, .rel = 1.3f, .amp = 1, .drive = 3}}},
    {"APPLAUS", "APL", 1, {{.wave = NO, .noise = 1, .crackle = 45, .att = 0.05f, .hold = 1.2f, .rel = 0.3f, .amp = 1.2f, .ftype = BP, .fc0 = 2200, .q = 0.8f}}},
    {"LAUGH", "LOL", 2, {{.wave = SAW, SEQ(S_LAUGH), .seql = 0.17f, .gate = 0.7f, .att = 0.01f, .amp = 2.2f, .noise = 0.4f, .ftype = BP, .fc0 = 800, .q = 3},
                          {.wave = SAW, SEQ(S_LAUGH), .seql = 0.17f, .gate = 0.7f, .att = 0.01f, .amp = 1.4f, .ftype = BP, .fc0 = 1400, .q = 4}}},
    {"GONG", "GNG", 4, {{.wave = SIN, .f = {110}, .att = 0.004f, .rel = 2.3f, .amp = 0.5f, .vibf = 0.7f, .vibd = 0.004f},
                         {.wave = SIN, .f = {164.3f}, .att = 0.004f, .rel = 2.05f, .amp = 0.35f, .vibf = 1, .vibd = 0.004f},
                         {.wave = SIN, .f = {237}, .att = 0.004f, .rel = 1.8f, .amp = 0.25f, .vibf = 1.3f, .vibd = 0.004f},
                         {.wave = SIN, .f = {302}, .att = 0.004f, .rel = 1.55f, .amp = 0.2f, .vibf = 1.6f, .vibd = 0.004f}}},
    {"TADA", "TDA", 2, {{.wave = SAW, .nosc = 3, .ratio = {659.0f / 523, 784.0f / 523}, .f = {523}, .att = 0.01f, .hold = 0.11f, .rel = 0.15f, .amp = 0.25f, .ftype = LP, .fc0 = 2500},
                         {.delay = 0.16f, .wave = SAW, .nosc = 4, .ratio = {659.0f / 523, 784.0f / 523, 2}, .f = {523}, .att = 0.01f, .hold = 0.7f, .rel = 0.15f, .amp = 0.25f, .ftype = LP, .fc0 = 2500}}},
    {"CLAP", "CLP", 2, {{.wave = NO, .noise = 1, .att = 0.001f, .rel = 0.01f, .amp = 1, .ftype = BP, .fc0 = 1500, .q = 1.2f, .reps = 3, .gap = 0.012f},
                         {.delay = 0.035f, .wave = NO, .noise = 1, .att = 0.001f, .rel = 0.21f, .amp = 0.8f, .ftype = BP, .fc0 = 1500, .q = 1.2f}}},
    {"SNARE", "SNR", 2, {{.wave = TRI, .f = {220, 120}, .t = {0.08f}, .att = 0.001f, .rel = 0.12f, .amp = 0.6f},
                          {.wave = NO, .noise = 1, .att = 0.001f, .rel = 0.18f, .amp = 0.7f, .ftype = HP, .fc0 = 1200}}},
    {"CRASH", "CRS", 2, {{.wave = NO, .noise = 1, .att = 0.001f, .rel = 1.4f, .amp = 0.6f, .ftype = HP, .fc0 = 5000, .q = 0.7f},
                          {.wave = SQ, .nosc = 4, .ratio = {512.0f / 340, 620.0f / 340, 887.0f / 340}, .f = {1020}, .att = 0.001f, .rel = 1, .amp = 0.08f, .ftype = HP, .fc0 = 6000}}},
    {"WHISTLE", "WHS", 2, {{.wave = SIN, .f = {2600}, .att = 0.02f, .hold = 0.6f, .rel = 0.05f, .amp = 0.3f, .vibf = 28, .vibd = 0.07f},
                            {.wave = NO, .noise = 1, .att = 0.02f, .hold = 0.6f, .rel = 0.05f, .amp = 0.2f, .ftype = BP, .fc0 = 2600, .q = 4}}},
    {"DUBSIRN", "DUB", 1, {{.wave = SQ, .f = {300, 1100}, .t = {0.35f}, .att = 0.01f, .hold = 0.34f, .rel = 0.02f, .amp = 0.3f, .ftype = LP, .fc0 = 3000, .reps = 4, .gap = 0.37f}}},
    {"CHEER", "CHR", 2, {{.wave = NO, .noise = 1, .att = 0.3f, .rel = 1.1f, .amp = 1.2f, .ftype = BP, .fc0 = 1200, .q = 1.5f},
                          {.wave = SAW, .nosc = 4, .ratio = {277.0f / 220, 330.0f / 220, 262.0f / 220}, .f = {220}, .att = 0.3f, .rel = 1.1f, .amp = 0.8f, .vibf = 6, .vibd = 0.03f, .ftype = BP, .fc0 = 700, .q = 3}}},
    {"COWBELL", "CWB", 1, {{.wave = SQ, .nosc = 2, .ratio = {800.0f / 540}, .f = {540}, .att = 0.001f, .rel = 0.35f, .amp = 0.8f, .ftype = BP, .fc0 = 800, .q = 3}}},
    {"CONFETTI", "CNF", 3, {{.wave = SIN, .f = {1400, 250}, .t = {0.04f}, .att = 0.001f, .rel = 0.06f, .amp = 0.9f},
                             {.wave = NO, .noise = 1, .att = 0.001f, .rel = 0.02f, .amp = 0.4f, .ftype = HP, .fc0 = 3000},
                             {.delay = 0.05f, .wave = SIN, SEQ(S_SPARK), .count = 12, .seqrand = 1, .seql = 0.045f, .gate = 0.4f, .att = 0.002f, .amp = 0.15f}}},
};
#define NMEME 96

static int meme_id(const char *name)
{
    for (int i = 0; i < NMEME; i++) {
        const char *a = MEMES[i].name, *b = name;
        while (*a && *a == *b) { a++; b++; }
        if (!*a && !*b) return i + 1;
    }
    return 0;
}
