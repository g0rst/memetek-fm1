/* SPDX-License-Identifier: GPL-3.0-only */
/* MEMETEK: an 8-bit tek groovebox for the M-VAVE FM-1, as an fm1-dock module.
 * Six tracks (TEK-KICK, HATS, 8-BIT BASS, CHIP ARP, 8-BIT LEAD, MEME FX), a 16-step sequencer with
 * four patterns, pages of parameters on the four knobs, a mixer, an echo, a bitcrusher.
 *
 * Panel (HB_F_PANEL):
 *   PLAY        start / stop            SEQ   STEP mode (white keys = steps) <-> PLAY mode (keys play)
 *   REC         record (running: in time; stopped: step input)
 *   SELECT      the track               ALGORITHM / EDIT   the page
 *   KNOB1..4    the page's parameters   PRESETS   the meme sound
 *   ENV         track pages             FX    mixer           GLO   tempo, swing, crush, drive
 *   ARP         dice: randomise the track   SEL + ARP   clear the track
 *   OCT- / OCT+ octave of the keys      SAVE  keep patterns and sounds in flash
 *   STEP mode, the upper row: OP1..OP6 mute KICK..MEME (SEL held: solo), PIT GLO MONO POLY pattern 1..4,
 *   the unlabelled key on the right: chain
 *
 * Float DSP on the FPU (-mfprev1), single precision only. render() runs in the audio interrupt. */
#include "hb_module.h"

#define FS 44100.0f
#define INV_FS (1.0f / 44100.0f)
#define NTR 6
#define NSTEP 16
#define NPAT 4

static const hb_host_t *H;

/* ------------------------------------------------------------------ math */
#define SIN_N 1024
static float SINT[SIN_N + 1];
static void sin_init(void)
{
    /* a rotation: cos/sin of 2pi/N, then N steps */
    const float c = 0.99998117528260111f, s = 0.0061358846491544753f;
    float x = 1.0f, y = 0.0f;
    for (int i = 0; i <= SIN_N; i++) {
        SINT[i] = y;
        float nx = x * c - y * s, ny = x * s + y * c;
        x = nx; y = ny;
        if ((i & 63) == 63) { float m = 1.5f - 0.5f * (x * x + y * y); x *= m; y *= m; }
    }
}
static inline float fsin1(float ph)                     /* ph in turns, any value */
{
    ph -= (float)(int)ph;
    if (ph < 0.0f) ph += 1.0f;
    float p = ph * (float)SIN_N;
    int i = (int)p;
    if (i >= SIN_N) i = SIN_N - 1;
    if (i < 0) i = 0;
    float f = p - (float)i;
    return SINT[i] + (SINT[i + 1] - SINT[i]) * f;
}
static inline float fexp2(float x)
{
    if (x < -120.0f) return 0.0f;
    if (x > 120.0f) x = 120.0f;
    int i = (int)x;
    if (x < (float)i) i--;
    float f = x - (float)i;
    float p = 1.0f + f * (0.6931472f + f * (0.2402265f + f * (0.0555041f + f * 0.0096181f)));
    union { float f; uint32_t u; } u;
    u.u = (uint32_t)(i + 127) << 23;
    return p * u.f;
}
static inline float fexp(float x) { return fexp2(x * 1.4426950f); }
static inline float fabsf_(float x) { return x < 0.0f ? -x : x; }
static inline float clampf(float x, float a, float b) { return x < a ? a : x > b ? b : x; }
static inline float ftanh(float x)
{
    if (x > 3.0f) return 1.0f;
    if (x < -3.0f) return -1.0f;
    float x2 = x * x;
    return x * (27.0f + x2) / (27.0f + 9.0f * x2);
}
static inline float ftan_(float x)                       /* 0 .. ~1.4 rad */
{
    float x2 = x * x;
    return x * (135135.0f - x2 * (17325.0f - x2 * (378.0f - x2))) /
           (135135.0f - x2 * (62370.0f - x2 * (3150.0f - 28.0f * x2)));
}
static float flog2(float r)
{
    float lg = 0.0f;
    if (r <= 0.0f) return -120.0f;
    while (r >= 2.0f) { r *= 0.5f; lg += 1.0f; }
    while (r < 1.0f) { r *= 2.0f; lg -= 1.0f; }
    float x = r - 1.0f;
    return lg + x * (1.4425449f - x * (0.7181452f - x * 0.2753700f));
}
static inline float semis(float s) { return fexp2(s * (1.0f / 12.0f)); }
static inline float mtof(float n) { return 440.0f * fexp2((n - 69.0f) * (1.0f / 12.0f)); }

static uint32_t rng = 0x1234567u;
static inline uint32_t rnd32(void) { rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5; return rng; }
static inline float frnd(void) { return (float)(int32_t)rnd32() * (1.0f / 2147483648.0f); }   /* -1..1 */
static inline float frnd01(void) { return (float)(rnd32() >> 8) * (1.0f / 16777216.0f); }

/* a decay factor per sample so that a level falls by `ratio` in `sec` seconds */
static float decay_k(float sec, float ratio_ln) { if (sec < 0.0005f) sec = 0.0005f; return fexp(ratio_ln / (sec * FS)); }
#define LN_1000 (-6.9077553f)

/* TPT state-variable filter */
typedef struct { float g, k, a1, a2, a3, ic1, ic2; } svf_t;
static void svf_set(svf_t *f, float hz, float q)
{
    hz = clampf(hz, 20.0f, 18000.0f);
    if (q < 0.3f) q = 0.3f;
    f->g = ftan_(3.14159265f * hz * INV_FS);
    f->k = 1.0f / q;
    f->a1 = 1.0f / (1.0f + f->g * (f->g + f->k));
    f->a2 = f->g * f->a1;
    f->a3 = f->g * f->a2;
}
/* mode 0 low, 1 band, 2 high */
static inline float svf_run(svf_t *f, float x, int mode)
{
    float v3 = x - f->ic2, v1 = f->a1 * f->ic1 + f->a2 * v3, v2 = f->ic2 + f->a2 * f->ic1 + f->a3 * v3;
    f->ic1 = 2.0f * v1 - f->ic1;
    f->ic2 = 2.0f * v2 - f->ic2;
    return mode == 0 ? v2 : mode == 1 ? v1 : x - f->k * v1 - v2;
}
static inline float qdb(float db) { return fexp2(db * (1.0f / 6.0206f)); }   /* Web Audio's LP/HP Q in dB */

static inline float wave(int w, float ph)                /* 0 square 1 pulse25 2 saw 3 tri 4 pulse12 5 sine */
{
    switch (w) {
    case 0: return ph < 0.5f ? 1.0f : -1.0f;
    case 1: return ph < 0.25f ? 1.0f : -1.0f;
    case 2: return 2.0f * ph - 1.0f;
    case 3: return 1.0f - 4.0f * fabsf_(ph - 0.5f);
    case 4: return ph < 0.125f ? 1.0f : -1.0f;
    default: return fsin1(ph);
    }
}
static inline float wrap1(float ph) { return ph >= 1.0f ? ph - (float)(int)ph : ph; }

/* ------------------------------------------------------------------ parameters */
enum { F_INT, F_HZ, F_S, F_MS, F_PCT, F_X, F_ST, F_LIST, F_Q, F_BPM, F_PM };
typedef struct {
    const char *name;
    float lo, hi, step, def;
    uint8_t fmt;
    const char *const *list;
    int8_t actx, aidx;                                   /* an alias: the value lives there */
} pdef_t;

static const char *const L_DIST[] = {"SOFT", "HARD", "FOLD"};
static const char *const L_HTYPE[] = {"METAL", "NOISE", "8-BIT", "SHAKER", "RIDE"};
static const char *const L_ROLL[] = {"2x", "3x", "4x", "6x"};
static const char *const L_ONOFF[] = {"AUS", "AN"};
static const char *const L_WAVE4[] = {"SQUARE", "PULSE", "SAW", "TRI"};
static const char *const L_NOTES[] = {"A", "A#", "B", "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#"};
static const char *const L_LTB[] = {"CUTOFF", "RESO", "PITCH", "F-ENV", "DECAY", "SUB", "VOL"};
static const char *const L_RATE[] = {"4BAR", "2BAR", "1BAR", "1/2", "1/4", "1/8", "1/16", "1/32"};
static const char *const L_SHAPE[] = {"SINE", "TRI", "SAW", "SQUARE", "S&H"};
static const char *const L_ARATE[] = {"2/STP", "3/STP", "4/STP", "6/STP"};
static const char *const L_DIR[] = {"UP", "DOWN", "PING", "RND"};
static const char *const L_CHORD[] = {"MOLL", "DUR", "SUS", "POWER"};
static const char *const L_LTC[] = {"PITCH", "VOL", "TONE", "PAN", "DECAY", "OKTAVE"};
static const char *const L_LTL[] = {"PITCH", "VOL", "TONE", "PAN"};
static const char *const L_LWAVE[] = {"SQUARE", "PULSE25", "PULSE12", "TRI", "SAW", "NOISE"};
static const char *const L_LARP[] = {"AUS", "OKTAVE", "DUR", "MOLL"};
static const char *const L_MMODE[] = {"NORMAL", "STUTTER"};
static const char *const L_ETIME[] = {"1/16", "1/8", "3/16", "1/4"};

#define P(n, lo, hi, st, d, f) {n, lo, hi, st, d, f, 0, -1, -1}
#define PL(n, hi, d, l) {n, 0, hi, 1, d, F_LIST, l, -1, -1}
#define PA(n, c, i) {n, 0, 0, 0, 0, 0, 0, c, i}
#define PN {0, 0, 0, 0, 0, 0, 0, -1, -1}

enum { K_TUNE, K_DECAY, K_DROP, K_DROPT, K_DRIVE, K_DTYPE, K_CLICK, K_VOL };
static const pdef_t D_KICK[] = {
    P("TUNE", 30, 80, 1, 42, F_HZ), P("DECAY", 0.15f, 0.9f, 0.01f, 0.45f, F_S), P("DROP", 2, 10, 0.1f, 5, F_X), P("DROP-T", 0.01f, 0.15f, 0.005f, 0.06f, F_MS),
    P("DRIVE", 0, 1, 0.01f, 0.3f, F_PCT), PL("DIST", 2, 0, L_DIST), P("CLICK", 0, 1, 0.01f, 0.6f, F_PCT), P("VOL", 0, 1, 0.01f, 0.9f, F_PCT)};
enum { H_TUNE, H_CDEC, H_ODEC, H_VOL, H_TYPE, H_ROLL, H_HUMAN, H_CHANCE, H_TONE, H_ACCENT, H_SPREAD, H_CHOKE };
static const pdef_t D_HAT[] = {
    P("TUNE", 0.5f, 2, 0.01f, 1, F_X), P("ZU", 0.01f, 0.1f, 0.005f, 0.035f, F_MS), P("OFFEN", 0.08f, 0.6f, 0.01f, 0.22f, F_MS), P("VOL", 0, 1, 0.01f, 0.3f, F_PCT),
    PL("TYP", 4, 0, L_HTYPE), PL("ROLL", 3, 1, L_ROLL), P("HUMANIZE", 0, 1, 0.01f, 0, F_PCT), P("CHANCE", 0, 1, 0.01f, 1, F_PCT),
    P("TONE", 0.3f, 1.5f, 0.01f, 1, F_X), P("ACCENT", 0, 1, 0.01f, 0, F_PCT), P("STEREO", 0, 1, 0.01f, 0, F_PCT), PL("CHOKE", 1, 1, L_ONOFF)};
enum { B_CUT, B_RES, B_FENV, B_DECAY, B_WAVE, B_SUB, B_GLIDE, B_VOL, B_ROOT, B_OCT, B_DUCK, B_DRIVE,
       B_L1T, B_L1R, B_L1D, B_L1S, B_L2T, B_L2R, B_L2D, B_L2S };
static const pdef_t D_BASS[] = {
    P("CUTOFF", 100, 5000, 10, 2400, F_HZ), P("RESO", 0.5f, 20, 0.1f, 6, F_Q), P("F-ENV", 0, 1, 0.01f, 0.8f, F_PCT), P("DECAY", 0.2f, 1.5f, 0.05f, 0.9f, F_X),
    PL("WAVE", 3, 0, L_WAVE4), P("SUB", 0, 1, 0.01f, 0.5f, F_PCT), P("GLIDE", 0, 0.2f, 0.005f, 0, F_MS), P("VOL", 0, 1, 0.01f, 0.5f, F_PCT),
    PL("ROOT", 11, 5, L_NOTES), P("OCT", -1, 1, 1, 0, F_ST), P("DUCK", 0, 1, 0.01f, 0, F_PCT), P("DRIVE", 0, 1, 0.01f, 0, F_PCT),
    PL("LFO1 ZIEL", 6, 0, L_LTB), PL("RATE", 7, 4, L_RATE), P("DEPTH", -1, 1, 0.01f, 0, F_PM), PL("FORM", 4, 0, L_SHAPE),
    PL("LFO2 ZIEL", 6, 2, L_LTB), PL("RATE", 7, 6, L_RATE), P("DEPTH", -1, 1, 0.01f, 0, F_PM), PL("FORM", 4, 0, L_SHAPE)};
enum { A_RATE, A_DIR, A_WAVE, A_VOL, A_OCT, A_DECAY, A_VIB, A_CHORD, A_LT, A_LR, A_LD, A_LS };
static const pdef_t D_ARP[] = {
    PL("RATE", 3, 2, L_ARATE), PL("RICHTG", 3, 0, L_DIR), PL("WAVE", 3, 0, L_WAVE4), P("VOL", 0, 0.6f, 0.01f, 0.22f, F_PCT),
    P("OCT", -1, 2, 1, 0, F_ST), P("DECAY", 0.2f, 1, 0.01f, 0.98f, F_PCT), P("VIBRATO", 0, 1, 0.01f, 0, F_PCT), PL("AKKORD", 3, 0, L_CHORD),
    PL("LFO ZIEL", 5, 0, L_LTC), PL("RATE", 7, 5, L_RATE), P("DEPTH", -1, 1, 0.01f, 0, F_PM), PL("FORM", 4, 0, L_SHAPE)};
enum { L_WAVE, L_LEN, L_VIB, L_VOL, L_OCT, L_GLIDE, L_SWEEP, L_DET, L_ATT, L_DEC, L_SUS, L_ARP, L_LT, L_LR, L_LD, L_LS };
static const pdef_t D_LEAD[] = {
    PL("WAVE", 5, 0, L_LWAVE), P("LAENGE", 0.25f, 4, 0.25f, 1, F_X), P("VIBRATO", 0, 1, 0.01f, 0, F_PCT), P("VOL", 0, 1, 0.01f, 0.35f, F_PCT),
    P("OCT", -2, 2, 1, 0, F_ST), P("GLIDE", 0, 0.3f, 0.005f, 0, F_MS), P("SWEEP", -12, 12, 1, 0, F_ST), P("DETUNE", 0, 1, 0.01f, 0, F_PCT),
    P("ATTACK", 0, 0.3f, 0.005f, 0, F_MS), P("DECAY", 0.05f, 2, 0.01f, 0.4f, F_S), P("SUSTAIN", 0, 1, 0.01f, 0.6f, F_PCT), PL("ARP", 3, 0, L_LARP),
    PL("LFO ZIEL", 3, 0, L_LTL), PL("RATE", 7, 4, L_RATE), P("DEPTH", -1, 1, 0.01f, 0, F_PM), PL("FORM", 4, 0, L_SHAPE)};
enum { M_PITCH, M_LEN, M_MODE, M_VOL, M_RND, M_ECHO, M_ETIME, M_FB };
static const pdef_t D_MEME[] = {
    P("PITCH", -12, 12, 1, 0, F_ST), P("LAENGE", 0.1f, 1, 0.01f, 1, F_PCT), PL("MODE", 1, 0, L_MMODE), P("VOL", 0, 1, 0.01f, 0.5f, F_PCT),
    P("RND-PITCH", 0, 12, 1, 0, F_INT), P("ECHO", 0, 1, 0.01f, 0, F_PCT), PL("E-ZEIT", 3, 1, L_ETIME), P("FEEDBK", 0, 0.9f, 0.01f, 0.4f, F_PCT)};
/* the mixer: volumes are the tracks' own (aliases) */
enum { X_MASTER = 6 };
static const pdef_t D_MIX[] = {
    PA("VOL KICK", 0, K_VOL), PA("VOL HATS", 1, H_VOL), PA("VOL BASS", 2, B_VOL), PA("VOL ARP", 3, A_VOL),
    PA("VOL LEAD", 4, L_VOL), PA("VOL MEME", 5, M_VOL), P("MASTER", 0, 1.2f, 0.01f, 0.8f, F_PCT), PN,
    P("PAN KICK", -1, 1, 0.05f, 0, F_PM), P("PAN HATS", -1, 1, 0.05f, 0, F_PM), P("PAN BASS", -1, 1, 0.05f, 0, F_PM), P("PAN ARP", -1, 1, 0.05f, 0, F_PM),
    P("PAN LEAD", -1, 1, 0.05f, 0, F_PM), P("PAN MEME", -1, 1, 0.05f, 0, F_PM), PA("E-ZEIT", 5, M_ETIME), PA("FEEDBK", 5, M_FB),
    P("ECHO KICK", 0, 1, 0.01f, 0, F_PCT), P("ECHO HATS", 0, 1, 0.01f, 0, F_PCT), P("ECHO BASS", 0, 1, 0.01f, 0, F_PCT), P("ECHO ARP", 0, 1, 0.01f, 0, F_PCT),
    P("ECHO LEAD", 0, 1, 0.01f, 0, F_PCT), PA("ECHO MEME", 5, M_ECHO), PN, PN};
/* pan i = D_MIX[8 + i] (index 8..13), send i: 16..21 (meme: alias) */
enum { G_BPM, G_SWING, G_CRUSH, G_DRIVE };
static const pdef_t D_GLO[] = {
    P("BPM", 60, 200, 1, 160, F_BPM), P("SWING", 0, 0.6f, 0.01f, 0, F_PCT), P("CRUSH", 0, 1, 0.01f, 0.45f, F_PCT), P("DRIVE", 0, 1, 0.01f, 0.35f, F_PCT)};

#define CTX_MIX 6
#define CTX_GLO 7
#define NCTX 8
#define MAXP 24
static const pdef_t *const DEFS[NCTX] = {D_KICK, D_HAT, D_BASS, D_ARP, D_LEAD, D_MEME, D_MIX, D_GLO};
static const uint8_t NDEF[NCTX] = {8, 12, 20, 12, 16, 8, 24, 4};
static float V[NCTX][MAXP];

static float *pval(int ctx, int i)
{
    const pdef_t *d = &DEFS[ctx][i];
    return d->actx >= 0 ? &V[(int)d->actx][(int)d->aidx] : &V[ctx][i];
}
static const pdef_t *pdef(int ctx, int i)
{
    const pdef_t *d = &DEFS[ctx][i];
    return d->actx >= 0 ? &DEFS[(int)d->actx][(int)d->aidx] : d;
}
static void params_default(int ctx)
{
    for (int i = 0; i < NDEF[ctx]; i++)
        if (DEFS[ctx][i].actx < 0) V[ctx][i] = DEFS[ctx][i].def;
}
#define PI_(c, i) ((int)(V[c][i] + 0.5f))

/* ------------------------------------------------------------------ memes (v0.1: 16 sounds) */
typedef struct {
    const char *name, *abbr;
    uint8_t wave;               /* 0 square 1 pulse 2 saw 3 tri 5 sine; 9: no oscillator */
    uint8_t nosc;               /* 1..4 */
    float ratio[3];             /* the other oscillators against the first */
    float f[4], t[3];           /* pitch: f0, then exp ramps to f1 at t0, f2 at +t1, f3 at +t2 (t 0: stop) */
    uint8_t nseq;               /* > 0: f[] ignored, seq[] notes, each seql long (the last one lastl) */
    float seq[8], seql, lastl;
    float att, hold, rel, amp;
    float vibf, vibd, vibdec;   /* vibrato Hz, depth (fraction of f), its fall time (0: steady) */
    float noise, noisedec;      /* a noise layer: level, decay */
    uint8_t ftype;              /* 0 none 1 LP 2 BP 3 HP */
    float fc0, fc1, q;          /* cutoff sweep over the sound */
    uint8_t reps;               /* repeats (1 = once) */
    float gap;                  /* repeat period */
    float drive;
} meme_t;

static const meme_t MEMES[] = {
    {"BOING", "BNG", 5, 1, {0}, {300, 700, 140, 0}, {0.07f, 0.48f, 0}, 0, {0}, 0, 0, 0.005f, 0.0f, 0.55f, 0.7f, 16, 0.25f, 0.5f, 0, 0, 0, 0, 0, 1, 1, 0, 0},
    {"SPRING", "SPR", 3, 1, {0}, {170, 260, 0, 0}, {1.1f, 0, 0}, 0, {0}, 0, 0, 0.003f, 0.0f, 1.1f, 0.7f, 24, 0.7f, 1.0f, 0, 0, 0, 0, 0, 1, 1, 0, 0},
    {"SLIDE UP", "SL^", 5, 1, {0}, {380, 1700, 0, 0}, {0.5f, 0, 0}, 0, {0}, 0, 0, 0.04f, 0.41f, 0.1f, 0.45f, 7, 0.02f, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0},
    {"SLIDE DN", "SLv", 5, 1, {0}, {1700, 330, 0, 0}, {0.6f, 0, 0}, 0, {0}, 0, 0, 0.04f, 0.51f, 0.1f, 0.45f, 7, 0.02f, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0},
    {"BONK", "BNK", 5, 1, {0}, {650, 140, 0, 0}, {0.09f, 0, 0}, 0, {0}, 0, 0, 0.001f, 0.0f, 0.18f, 1.0f, 0, 0, 0, 0.5f, 0.04f, 0, 0, 0, 0, 1, 0, 0},
    {"PEW", "PEW", 0, 1, {0}, {1700, 160, 0, 0}, {0.2f, 0, 0}, 0, {0}, 0, 0, 0.002f, 0.0f, 0.2f, 0.5f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0},
    {"COIN", "CON", 0, 1, {0}, {0}, {0}, 2, {988, 1319}, 0.07f, 0.35f, 0.002f, 0.0f, 0.4f, 0.45f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0},
    {"JUMP", "JMP", 0, 1, {0}, {280, 950, 0, 0}, {0.16f, 0, 0}, 0, {0}, 0, 0, 0.002f, 0.0f, 0.2f, 0.4f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0},
    {"1UP", "1UP", 0, 1, {0}, {0}, {0}, 5, {523, 659, 784, 1047, 1319}, 0.07f, 0.07f, 0.001f, 0.0f, 0.36f, 0.35f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0},
    {"LASER", "LSR", 2, 1, {0}, {2800, 90, 0, 0}, {0.35f, 0, 0}, 0, {0}, 0, 0, 0.002f, 0.0f, 0.35f, 0.4f, 40, 0.07f, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0},
    {"HORN", "HRN", 2, 4, {470.0f / 466.0f, 698.0f / 466.0f, 703.0f / 466.0f}, {466, 0, 0, 0}, {0}, 0, {0}, 0, 0, 0.015f, 0.3f, 0.08f, 0.35f, 0, 0, 0, 0, 0, 2, 1300, 1300, 0.8f, 1, 0, 0},
    {"BOOM", "BOM", 5, 1, {0}, {120, 30, 0, 0}, {0.5f, 0, 0}, 0, {0}, 0, 0, 0.002f, 0.0f, 1.5f, 1.0f, 0, 0, 0, 0.8f, 0.25f, 0, 0, 0, 0, 1, 0, 3.0f},
    {"FART", "FRT", 2, 1, {0}, {85, 110, 60, 95}, {0.12f, 0.14f, 0.2f}, 0, {0}, 0, 0, 0.02f, 0.0f, 0.45f, 0.9f, 23, 0.25f, 0, 0.3f, 0.4f, 1, 450, 450, 7, 1, 0, 0},
    {"SAD", "SAD", 2, 1, {0}, {0}, {0}, 4, {311, 294, 277, 262}, 0.32f, 1.0f, 0.04f, 0.0f, 0.06f, 0.6f, 6, 0.03f, 0, 0, 0, 1, 1000, 1000, 2, 1, 0, 0},
    {"BRUH", "BRH", 2, 1, {0}, {125, 88, 0, 0}, {0.45f, 0, 0}, 0, {0}, 0, 0, 0.015f, 0.0f, 0.45f, 2.5f, 0, 0, 0, 0, 0, 2, 350, 350, 4, 1, 0, 0},
    {"QUACK", "QAK", 2, 1, {0}, {420, 300, 0, 0}, {0.17f, 0, 0}, 0, {0}, 0, 0, 0.005f, 0.0f, 0.17f, 2.0f, 0, 0, 0, 0, 0, 2, 1600, 800, 3, 1, 0, 0},
};
#define NMEME ((int)(sizeof MEMES / sizeof MEMES[0]))

/* ------------------------------------------------------------------ patterns */
typedef struct { uint8_t s[NTR][NSTEP]; } pat_t;
static pat_t PAT[NPAT];
enum { T_KICK, T_HAT, T_BASS, T_ARP, T_LEAD, T_MEME };
static const char *const TNAME[NTR] = {"TEK-KICK", "HI-HATS", "8BIT BASS", "CHIP ARP", "8BIT LEAD", "MEME FX"};
static const char TLET[NTR] = {'K', 'H', 'B', 'A', 'L', 'M'};
static const uint16_t TCOL[NTR] = {HB_RGB(255, 77, 77), HB_RGB(54, 200, 255), HB_RGB(255, 176, 46),
                                   HB_RGB(62, 224, 143), HB_RGB(255, 111, 168), HB_RGB(200, 107, 255)};

static void patterns_default(void)
{
    static const uint8_t KICK[NSTEP] = {2, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0};
    static const uint8_t KICK2[NSTEP] = {2, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 1, 0};
    static const uint8_t HAT[NSTEP] = {0, 0, 2, 0, 0, 0, 2, 0, 0, 0, 2, 0, 0, 0, 2, 0};
    static const uint8_t HAT2[NSTEP] = {1, 1, 2, 1, 1, 1, 2, 1, 1, 1, 2, 1, 1, 3, 2, 3};
    static const uint8_t BASS[NSTEP] = {0, 1, 1, 1, 0, 1, 1, 2, 0, 1, 1, 1, 0, 1, 3, 2};
    static const uint8_t ARP2[NSTEP] = {1, 0, 0, 1, 0, 0, 1, 0, 2, 0, 0, 2, 0, 0, 3, 0};
    static const uint8_t ARP3[NSTEP] = {1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 1, 1, 2, 3};
    static const uint8_t LEAD3[NSTEP] = {62, 0, 65, 0, 69, 0, 67, 65, 64, 0, 62, 0, 57, 0, 60, 62};
    for (int p = 0; p < NPAT; p++)
        for (int t = 0; t < NTR; t++)
            for (int i = 0; i < NSTEP; i++) PAT[p].s[t][i] = 0;
    for (int i = 0; i < NSTEP; i++) {
        PAT[0].s[T_KICK][i] = KICK[i]; PAT[0].s[T_HAT][i] = HAT[i]; PAT[0].s[T_BASS][i] = BASS[i];
        PAT[1].s[T_KICK][i] = KICK2[i]; PAT[1].s[T_HAT][i] = HAT2[i]; PAT[1].s[T_BASS][i] = BASS[i]; PAT[1].s[T_ARP][i] = ARP2[i];
        PAT[2].s[T_KICK][i] = (i == 0 || i == 6 || i == 8) ? 1 : 0; PAT[2].s[T_HAT][i] = (i & 1) ? 0 : 1;
        PAT[2].s[T_ARP][i] = ARP3[i]; PAT[2].s[T_LEAD][i] = LEAD3[i];
        PAT[2].s[T_BASS][i] = (i & 1) ? 0 : (uint8_t)(i < 8 ? 1 : i < 12 ? 2 : 3);
        PAT[3].s[T_KICK][i] = KICK[i]; PAT[3].s[T_HAT][i] = HAT[i];
    }
    PAT[0].s[T_MEME][14] = 11;                           /* HORN */
    PAT[1].s[T_MEME][0] = 15; PAT[1].s[T_MEME][6] = 1; PAT[1].s[T_MEME][10] = 16; PAT[1].s[T_MEME][12] = 13; PAT[1].s[T_MEME][14] = 11;
    PAT[1].s[T_LEAD][8] = 74; PAT[1].s[T_LEAD][10] = 72; PAT[1].s[T_LEAD][12] = 69;
    PAT[2].s[T_MEME][3] = 8; PAT[2].s[T_MEME][7] = 7; PAT[2].s[T_MEME][11] = 8; PAT[2].s[T_MEME][15] = 9;
    PAT[3].s[T_MEME][0] = 12; PAT[3].s[T_MEME][4] = 2; PAT[3].s[T_MEME][8] = 5; PAT[3].s[T_MEME][12] = 14;
}

/* ------------------------------------------------------------------ shared state (main loop <-> audio) */
static volatile uint8_t playing, rec, chain_on;
static volatile uint8_t pat_edit, pat_play, pat_next;
static volatile uint8_t mute[NTR], solo[NTR];
static volatile int cur_step = -1;
static volatile uint32_t step_count;
static float sps = 4134.375f;                            /* samples per step */
static float lfo_pos;                                    /* transport position in steps, wraps at 64 */

/* requests from the main loop, taken at the start of render() */
#define NREQ 16
typedef struct { uint8_t kind, a, b; } req_t;
static req_t reqq[NREQ];
static volatile uint32_t req_w, req_r;
enum { R_LEAD_ON = 1, R_LEAD_OFF, R_MEME, R_KICK, R_HAT, R_BASS, R_ARP, R_START, R_STOP, R_PANIC };
static void post(uint8_t kind, uint8_t a, uint8_t b)
{
    uint32_t w = req_w;
    if (w - req_r >= NREQ) return;
    reqq[w % NREQ].kind = kind; reqq[w % NREQ].a = a; reqq[w % NREQ].b = b;
    __asm__ volatile("" ::: "memory");
    req_w = w + 1;
}

/* scheduled events, in samples from the start of this block */
#define NEV 32
typedef struct { int32_t at; uint8_t kind, a, b; } ev_t;
static ev_t evq[NEV];
static int nev;
enum { E_HAT = 1, E_LEAD_OFF, E_MEME };
static void ev_add(int32_t at, uint8_t kind, uint8_t a, uint8_t b)
{
    if (nev >= NEV) return;
    evq[nev].at = at; evq[nev].kind = kind; evq[nev].a = a; evq[nev].b = b; nev++;
}

/* ------------------------------------------------------------------ track buffers */
#define BLK 256
static float TL[NTR][BLK], TR_[NTR][BLK];
static int16_t echo_buf[32768];
static uint32_t echo_w;

/* LFO */
static const float LRST[8] = {64, 32, 16, 8, 4, 2, 1, 0.5f};
static inline float lhash(int n) { uint32_t x = (uint32_t)n * 2654435761u; x ^= x >> 15; x *= 2246822519u; x ^= x >> 13; return (float)(x >> 8) * (1.0f / 8388608.0f) - 1.0f; }
static float lfo_shape(int s, float ph, int salt)
{
    int ip = (int)ph; if (ph < (float)ip) ip--;
    float p = ph - (float)ip;
    switch (s) {
    case 0: return fsin1(p);
    case 1: return 1.0f - 4.0f * fabsf_(p - 0.5f);
    case 2: return 2.0f * p - 1.0f;
    case 3: return p < 0.5f ? 1.0f : -1.0f;
    default: return lhash(ip + salt);
    }
}
static float lfo_val(int ctx, int ti, int ri, int di, int si, float pos, int salt, int *target)
{
    float d = V[ctx][di];
    *target = PI_(ctx, ti);
    if (d == 0.0f) return 0.0f;
    return lfo_shape(PI_(ctx, si), pos / LRST[PI_(ctx, ri)], salt) * d;
}

/* ------------------------------------------------------------------ KICK */
static struct { int on; float ph, f, fend, kf, amp, ka, click, kc, hpx, hpy; int hold; } kick;
static float duck_g = 1.0f;
static void kick_trig(int acc)
{
    float tail = V[0][K_DECAY];
    kick.on = 1; kick.ph = 0.0f;
    kick.fend = V[0][K_TUNE];
    kick.f = kick.fend * V[0][K_DROP] * (acc ? 1.3f : 1.0f);
    kick.kf = decay_k(V[0][K_DROPT] * 0.33f, -1.0f);
    kick.amp = acc ? 1.3f : 1.0f;
    kick.hold = (int)(tail * 0.35f * FS);
    kick.ka = decay_k(tail * 0.65f, LN_1000);
    kick.click = V[0][K_CLICK];
    kick.kc = decay_k(0.002f, -1.0f);
    if (V[2][B_DUCK] > 0.0f) duck_g = 1.0f - V[2][B_DUCK];
}
static inline float kick_shape(float x)
{
    float d = V[0][K_DRIVE];
    x = clampf(x, -1.0f, 1.0f);
    switch (PI_(0, K_DTYPE)) {
    case 1: return clampf(x * (1.0f + d * 14.0f) * 0.7f, -1.0f, 1.0f);
    case 2: return fsin1(x * (1.0f + d * 5.0f) * 0.25f);
    default: { float k = 1.0f + d * 14.0f; return ftanh(k * x) / ftanh(k); }
    }
}
static void kick_run(float *l, float *r, int a, int b)
{
    if (!kick.on) return;
    for (int i = a; i < b; i++) {
        kick.f = kick.fend + (kick.f - kick.fend) * kick.kf;
        kick.ph = wrap1(kick.ph + kick.f * INV_FS);
        float s = fsin1(kick.ph) * kick.amp;
        if (kick.hold > 0) kick.hold--; else kick.amp *= kick.ka;
        if (kick.click > 0.0005f) {
            float n = frnd() * kick.click;
            kick.click *= kick.kc;
            kick.hpy = 0.70f * (kick.hpy + n - kick.hpx); kick.hpx = n;
            s += kick.hpy;
        }
        s = kick_shape(s);
        l[i] += s; r[i] += s;
    }
    if (kick.amp < 0.0005f) kick.on = 0;
}

/* ------------------------------------------------------------------ HATS */
#define NHAT 4
static const float HATF[6] = {263, 400, 421, 474, 587, 845};
typedef struct { int on, type, att; float amp, peak, kd, gl, gr, sh, shc, shr; float ph[6], inc[6]; svf_t bp, hp; } hat_t;
static hat_t hats[NHAT];
static int hat_rr;
static void hat_hit(int lv, float velmul, int step)
{
    int open = lv == 2, T = PI_(1, H_TYPE);
    float tn = V[1][H_TUNE], vel = velmul * (1.0f - V[1][H_HUMAN] * frnd01() * 0.7f);
    if (V[1][H_ACCENT] > 0.0f && step >= 0)
        vel *= (step % 4 == 2) ? 1.0f + V[1][H_ACCENT] * 0.6f : (step & 1) ? 1.0f - V[1][H_ACCENT] * 0.45f : 1.0f;
    float dec = (open ? V[1][H_ODEC] : V[1][H_CDEC]) * (T == 4 ? 3.0f : T == 3 ? 1.4f : 1.0f);
    if (PI_(1, H_CHOKE) && !open)
        for (int v = 0; v < NHAT; v++)
            if (hats[v].on == 2) { hats[v].kd = decay_k(0.006f, -1.0f); hats[v].on = 1; }
    hat_t *h = &hats[hat_rr++ % NHAT];
    h->on = open ? 2 : 1; h->type = T;
    h->peak = (open ? 0.7f : 0.9f) * vel;
    h->att = T == 3 ? (int)(0.012f * FS) : 0;
    h->amp = T == 3 ? 0.0f : h->peak;
    h->kd = decay_k(dec, LN_1000);
    float p = V[1][H_SPREAD] > 0.0f ? frnd() * V[1][H_SPREAD] : 0.0f;
    h->gl = p > 0.0f ? 1.0f - p : 1.0f; h->gr = p < 0.0f ? 1.0f + p : 1.0f;
    for (int k = 0; k < 6; k++) { h->inc[k] = HATF[k] * 1.6f * tn * (T == 4 ? 0.8f : 1.0f) * INV_FS; h->ph[k] = frnd01(); }
    svf_set(&h->hp, 7500.0f * tn * V[1][H_TONE], 1.0f);
    svf_set(&h->bp, T == 3 ? 5200.0f * tn : (T == 4 ? 6000.0f : 10000.0f) * tn, T == 3 ? 1.5f : 1.0f);
    h->hp.ic1 = h->hp.ic2 = h->bp.ic1 = h->bp.ic2 = 0.0f;
    h->shr = 0.18f * tn; h->shc = 0.0f; h->sh = 0.0f;
}
static void hat_trig(int lv, int step)
{
    if (V[1][H_CHANCE] < 1.0f && frnd01() > V[1][H_CHANCE]) return;
    if (lv == 3) {
        int n = (const int[]){2, 3, 4, 6}[PI_(1, H_ROLL)];
        float sub = sps / (float)n;
        hat_hit(1, 0.5f + 0.5f / (float)n, step);
        for (int i = 1; i < n; i++) ev_add((int32_t)(sub * (float)i), E_HAT, (uint8_t)(i * 255 / n), (uint8_t)n);
        return;
    }
    hat_hit(lv, 1.0f, step);
}
static void hats_run(float *l, float *r, int a, int b)
{
    for (int v = 0; v < NHAT; v++) {
        hat_t *h = &hats[v];
        if (!h->on) continue;
        for (int i = a; i < b; i++) {
            float x;
            switch (h->type) {
            case 0: case 4: {
                float m = 0.0f;
                for (int k = 0; k < 6; k++) { h->ph[k] = wrap1(h->ph[k] + h->inc[k]); m += h->ph[k] < 0.5f ? 1.0f : -1.0f; }
                x = svf_run(&h->bp, m * 0.12f + frnd() * 0.5f, 1);
                break;
            }
            case 2:
                h->shc += h->shr;
                if (h->shc >= 1.0f) { h->shc -= 1.0f; h->sh = frnd(); }
                x = h->sh;
                break;
            case 3: x = svf_run(&h->bp, frnd(), 1); break;
            default: x = frnd(); break;
            }
            x = svf_run(&h->hp, x, 2);
            if (h->att > 0) { h->amp += h->peak / (0.012f * FS); h->att--; } else h->amp *= h->kd;
            x *= h->amp;
            l[i] += x * h->gl; r[i] += x * h->gr;
        }
        if (!h->att && h->amp < 0.0005f) h->on = 0;
    }
}

/* ------------------------------------------------------------------ BASS */
static const int IV[4] = {0, 0, 7, 12};
static struct { int on, att, cr; float ph, phs, f, ft, kg, amp, peak, ka, cut, kc, decm, fenv; svf_t lp; } bass;
static float bass_lastf;
static inline float bfreq(int semi) { return 55.0f * semis((float)semi); }
static void bass_trig(int lv)
{
    int t1, t2;
    float m1 = lfo_val(2, B_L1T, B_L1R, B_L1D, B_L1S, lfo_pos, 1000, &t1);
    float m2 = lfo_val(2, B_L2T, B_L2R, B_L2D, B_L2S, lfo_pos, 2000, &t2);
    float mdec = (t1 == 4 ? m1 : 0) + (t2 == 4 ? m2 : 0), mfe = (t1 == 3 ? m1 : 0) + (t2 == 3 ? m2 : 0);
    float f = bfreq(PI_(2, B_ROOT) + 12 * PI_(2, B_OCT) + IV[lv & 3]);
    bass.decm = clampf(V[2][B_DECAY] * (1.0f + mdec), 0.1f, 2.0f);
    float ln = sps * INV_FS * bass.decm;
    bass.ft = f;
    if (V[2][B_GLIDE] > 0.0f && bass_lastf > 0.0f) { bass.f = bass_lastf; bass.kg = decay_k(V[2][B_GLIDE] * 0.33f, -1.0f); }
    else { bass.f = f; bass.kg = 0.0f; }
    bass_lastf = f;
    bass.on = 1; bass.att = (int)(0.003f * FS); bass.peak = 0.8f; bass.amp = 0.0f;
    bass.ka = decay_k(ln * 0.95f, LN_1000);
    bass.fenv = clampf(V[2][B_FENV] + mfe, 0.0f, 1.0f);
    bass.cut = 1.0f;                                     /* the envelope, as a factor of CUTOFF: 1 -> 1 - fenv*.92 */
    float endf = 1.0f - bass.fenv * 0.92f;
    if (endf < 0.02f) endf = 0.02f;
    float upd = ln * 0.8f * FS / 16.0f;
    if (upd < 1.0f) upd = 1.0f;
    bass.kc = fexp2(flog2(endf) / upd);
    bass.cr = 0;
}
static void bass_run(float *l, float *r, int a, int b)
{
    if (!bass.on) return;
    int w = PI_(2, B_WAVE);
    static float sub = 0, vol = 1, pm = 1;
    float k = 0.5f + V[2][B_DRIVE] * 10.0f, nk = 1.0f / ftanh(k);
    for (int i = a; i < b; i++) {
        if ((bass.cr++ & 15) == 0) {
            int t1, t2;
            float pos = lfo_pos + (float)(i - a) / sps;
            float m1 = lfo_val(2, B_L1T, B_L1R, B_L1D, B_L1S, pos, 1000, &t1);
            float m2 = lfo_val(2, B_L2T, B_L2R, B_L2D, B_L2S, pos, 2000, &t2);
            float mc = (t1 == 0 ? m1 : 0) + (t2 == 0 ? m2 : 0), mr = (t1 == 1 ? m1 : 0) + (t2 == 1 ? m2 : 0);
            float mp = (t1 == 2 ? m1 : 0) + (t2 == 2 ? m2 : 0), ms = (t1 == 5 ? m1 : 0) + (t2 == 5 ? m2 : 0);
            float mv = (t1 == 6 ? m1 : 0) + (t2 == 6 ? m2 : 0);
            float fc = clampf(V[2][B_CUT] * fexp2(3.0f * mc), 60.0f, 12000.0f) * bass.cut;
            svf_set(&bass.lp, fc < 50.0f ? 50.0f : fc, qdb(clampf(V[2][B_RES] + mr * 10.0f, 0.5f, 25.0f)));
            pm = mp != 0.0f ? semis(mp * 12.0f) : 1.0f;
            sub = clampf(V[2][B_SUB] + ms, 0.0f, 1.0f);
            vol = clampf(1.0f + mv, 0.0f, 2.0f);
            bass.cut *= bass.kc;
            float floorc = 1.0f - bass.fenv * 0.92f;
            if (bass.cut < floorc) bass.cut = floorc;
        }
        if (bass.kg > 0.0f) bass.f = bass.ft + (bass.f - bass.ft) * bass.kg;
        float f = bass.f * pm;
        bass.ph = wrap1(bass.ph + f * INV_FS);
        bass.phs = wrap1(bass.phs + f * 0.5f * INV_FS);
        float x = svf_run(&bass.lp, wave(w, bass.ph), 0) + wave(3, bass.phs) * sub;
        if (bass.att > 0) { bass.amp += bass.peak / (0.003f * FS); bass.att--; } else bass.amp *= bass.ka;
        x *= bass.amp * vol;
        x = ftanh(k * x) * nk * duck_g;
        l[i] += x; r[i] += x;
    }
    if (!bass.att && bass.amp < 0.0005f) bass.on = 0;
}

/* ------------------------------------------------------------------ CHIP ARP */
static const int CHORDS[4][4] = {{0, 3, 7, 12}, {0, 4, 7, 12}, {0, 5, 7, 12}, {0, 7, 12, 19}};
static const int CHL[4] = {0, 0, 5, 7};
static struct { int on, n, idx, sub, cnt, cr; float ph, f[6], amp, ka, vph, lpan; svf_t lp; int tg; } arp;
static void arp_trig(int lv)
{
    int tg;
    float v0 = lfo_val(3, A_LT, A_LR, A_LD, A_LS, lfo_pos, 555, &tg);
    int octj = (tg == 5 && V[3][A_LD] != 0.0f) ? (int)(v0 * 2.0f + (v0 < 0 ? -0.5f : 0.5f)) : 0;
    float decm = (tg == 4 && V[3][A_LD] != 0.0f) ? clampf(1.0f + v0, 0.15f, 2.0f) : 1.0f;
    int base = PI_(2, B_ROOT) + 36 + 12 * (PI_(3, A_OCT) + octj) + CHL[lv & 3];
    const int *ch = CHORDS[PI_(3, A_CHORD)];
    int n = (const int[]){2, 3, 4, 6}[PI_(3, A_RATE)], dir = PI_(3, A_DIR);
    for (int i = 0; i < n; i++) {
        int k;
        if (dir == 0) k = i % 4;
        else if (dir == 1) k = 3 - i % 4;
        else if (dir == 2) { int j = i % 6; k = j < 4 ? j : 6 - j; }
        else k = (int)(rnd32() & 3);
        arp.f[i] = bfreq(base + ch[k]);
    }
    float ln = sps * INV_FS * clampf(V[3][A_DECAY] * decm, 0.1f, 2.0f);
    arp.on = 1; arp.n = n; arp.idx = 0; arp.sub = (int)(sps / (float)n); arp.cnt = 0; arp.cr = 0;
    arp.amp = 0.9f; arp.ka = decay_k(ln * 0.98f, LN_1000);
    arp.lp.ic1 = arp.lp.ic2 = 0.0f;
}
static void arp_run(float *l, float *r, int a, int b)
{
    if (!arp.on) return;
    int w = PI_(3, A_WAVE);
    static float pm = 1, vm = 1, gl = 1, gr = 1;
    static int tone;
    for (int i = a; i < b; i++) {
        if ((arp.cr++ & 15) == 0) {
            int tg;
            float v = lfo_val(3, A_LT, A_LR, A_LD, A_LS, lfo_pos + (float)(i - a) / sps, 555, &tg);
            int on = V[3][A_LD] != 0.0f;
            pm = (on && tg == 0) ? semis(v * 12.0f) : 1.0f;
            if (V[3][A_VIB] > 0.0f) pm *= semis(fsin1(arp.vph) * V[3][A_VIB] * 0.6f);
            vm = (on && tg == 1) ? clampf(1.0f + v, 0.0f, 2.0f) : 1.0f;
            tone = on && tg == 2;
            if (tone) svf_set(&arp.lp, clampf(2500.0f * fexp2(v * 2.3f), 200.0f, 16000.0f), qdb(2.0f));
            float p = (on && tg == 3) ? clampf(v, -1.0f, 1.0f) : 0.0f;
            gl = p > 0 ? 1.0f - p : 1.0f; gr = p < 0 ? 1.0f + p : 1.0f;
        }
        arp.vph += 6.0f * INV_FS;
        if (++arp.cnt >= arp.sub && arp.idx < arp.n - 1) { arp.cnt = 0; arp.idx++; }
        arp.ph = wrap1(arp.ph + arp.f[arp.idx] * pm * INV_FS);
        float x = wave(w, arp.ph) * arp.amp * vm;
        if (tone) x = svf_run(&arp.lp, x, 0);
        arp.amp *= arp.ka;
        l[i] += x * gl; r[i] += x * gr;
    }
    if (arp.amp < 0.0005f) arp.on = 0;
}

/* ------------------------------------------------------------------ 8-BIT LEAD */
static float NOISE64[64];
static const int LARP[4][3] = {{0, 0, 0}, {0, 12, 12}, {0, 4, 7}, {0, 3, 7}};
static const int LARPN[4] = {1, 2, 3, 3};
static struct { int on, gate, note, arpi, arpc, cr, stage; float ph1, ph2, f, ft, kg, amp, vph, kdec, krel; svf_t lp; } lead;
static float lead_lastf;
static void lead_on(int note)
{
    float f = mtof((float)(note + 12 * PI_(4, L_OCT)));
    if (V[4][L_GLIDE] > 0.0f && lead_lastf > 0.0f && lead.gate) { lead.f = lead_lastf; lead.kg = decay_k(V[4][L_GLIDE] * 0.33f, -1.0f); }
    else if (V[4][L_SWEEP] != 0.0f) { lead.f = f * semis(V[4][L_SWEEP]); lead.kg = decay_k(0.02f, -1.0f); }
    else { lead.f = f; lead.kg = 0.0f; }
    lead.ft = f; lead_lastf = f;
    lead.on = 1; lead.gate = 1; lead.note = note; lead.stage = 0;
    if (!(V[4][L_GLIDE] > 0.0f)) lead.amp = 0.0f;
    lead.kdec = decay_k(V[4][L_DEC] * 0.33f, -1.0f);
    lead.krel = decay_k(0.02f, -1.0f);
    lead.arpi = 0; lead.arpc = 0;
}
static void lead_off(int note)
{
    if (lead.gate && (note < 0 || note == lead.note)) lead.gate = 0;
}
static void lead_run(float *l, float *r, int a, int b)
{
    if (!lead.on) return;
    int w = PI_(4, L_WAVE), arpm = PI_(4, L_ARP);
    float det = V[4][L_DET], att = V[4][L_ATT], sus = V[4][L_SUS];
    static float pm = 1, vm = 1, gl = 1, gr = 1;
    static int tone;
    int arps = (int)(sps * 0.25f);
    for (int i = a; i < b; i++) {
        if ((lead.cr++ & 15) == 0) {
            int tg;
            float v = lfo_val(4, L_LT, L_LR, L_LD, L_LS, lfo_pos + (float)(i - a) / sps, 999, &tg);
            int on = V[4][L_LD] != 0.0f;
            pm = (on && tg == 0) ? semis(v * 12.0f) : 1.0f;
            if (V[4][L_VIB] > 0.0f) pm *= semis(fsin1(lead.vph) * V[4][L_VIB] * 0.45f);
            vm = (on && tg == 1) ? clampf(1.0f + v, 0.0f, 2.0f) : 1.0f;
            tone = on && tg == 2;
            if (tone) svf_set(&lead.lp, clampf(2500.0f * fexp2(v * 2.3f), 200.0f, 16000.0f), qdb(2.0f));
            float p = (on && tg == 3) ? clampf(v, -1.0f, 1.0f) : 0.0f;
            gl = p > 0 ? 1.0f - p : 1.0f; gr = p < 0 ? 1.0f + p : 1.0f;
        }
        lead.vph += 6.0f * INV_FS;
        if (lead.kg > 0.0f) lead.f = lead.ft + (lead.f - lead.ft) * lead.kg;
        float f = lead.f * pm;
        if (arpm) {
            if (++lead.arpc >= arps) { lead.arpc = 0; lead.arpi = (lead.arpi + 1) % LARPN[arpm]; }
            f *= semis((float)LARP[arpm][lead.arpi]);
        }
        float x;
        if (w == 5) {
            lead.ph1 = wrap1(lead.ph1 + f * INV_FS);
            x = NOISE64[(int)(lead.ph1 * 64.0f) & 63];
        } else {
            static const int WMAP[5] = {0, 1, 4, 3, 2};
            float d = det * 0.0116f;                    /* ~20 cents */
            lead.ph1 = wrap1(lead.ph1 + f * (1.0f - d) * INV_FS);
            x = wave(WMAP[w], lead.ph1);
            if (det > 0.0f) { lead.ph2 = wrap1(lead.ph2 + f * (1.0f + d) * INV_FS); x = 0.5f * (x + wave(WMAP[w], lead.ph2)); }
        }
        /* envelope: attack to .5, then towards .5 * sustain; release */
        if (lead.gate) {
            if (lead.stage == 0) {
                lead.amp += att > 0.001f ? 0.5f / (att * FS) : 0.5f;
                if (lead.amp >= 0.5f) { lead.amp = 0.5f; lead.stage = 1; }
            } else lead.amp = 0.5f * sus + (lead.amp - 0.5f * sus) * lead.kdec;
        } else lead.amp *= lead.krel;
        if (tone) x = svf_run(&lead.lp, x, 0);
        x *= lead.amp * vm;
        l[i] += x * gl; r[i] += x * gr;
    }
    if (!lead.gate && lead.amp < 0.0003f) lead.on = 0;
}

/* ------------------------------------------------------------------ MEME voices */
#define NMV 4
typedef struct {
    int on, def, seg, si, reps;
    int32_t t, tseg, tlen, rep_t;
    float f, kf, fnext, amp, vibd, kvib, vph, noise, kn, cut, kcut, pitch, timesc;
    float ph[4];
    svf_t flt;
    int cr;
} mv_t;
static mv_t mv[NMV];
static int mv_rr;
static float meme_ramp_k(float f0, float f1, float sec)
{
    if (f0 <= 0.0f || f1 <= 0.0f || sec <= 0.0f) return 1.0f;
    /* f0 * k^(sec*FS) = f1  ->  k = 2^(log2(f1/f0) / (sec*FS)) */
    return fexp2(flog2(f1 / f0) / (sec * FS));
}
static void mv_seg(mv_t *v)
{
    const meme_t *m = &MEMES[v->def];
    float ts = v->timesc;
    if (m->nseq) {
        if (v->si >= m->nseq) { v->tseg = 0x7fffffff; return; }
        v->f = m->seq[v->si] * v->pitch; v->kf = 1.0f;
        v->tseg = v->t + (int32_t)((v->si == m->nseq - 1 ? m->lastl : m->seql) * ts * FS);
        v->si++;
        return;
    }
    if (v->seg < 3 && m->t[v->seg] > 0.0f && m->f[v->seg + 1] > 0.0f) {
        v->kf = meme_ramp_k(m->f[v->seg], m->f[v->seg + 1], m->t[v->seg] * ts);
        v->tseg = v->t + (int32_t)(m->t[v->seg] * ts * FS);
        v->seg++;
    } else { v->kf = 1.0f; v->tseg = 0x7fffffff; }
}
static void meme_trig(int idx, int step)
{
    if (idx < 1 || idx > NMEME) return;
    const meme_t *m = &MEMES[idx - 1];
    mv_t *v = &mv[mv_rr++ % NMV];
    float pitch = V[5][M_PITCH] + (V[5][M_RND] > 0.0f ? (float)(int)(frnd() * V[5][M_RND]) : 0.0f);
    v->def = idx - 1; v->on = 1; v->t = 0; v->seg = 0; v->si = 0; v->cr = 0;
    v->pitch = semis(pitch);
    v->timesc = V[5][M_LEN];
    v->f = (m->nseq ? m->seq[0] : m->f[0]) * v->pitch;
    v->tlen = (int32_t)((m->att + m->hold + m->rel) * v->timesc * FS);
    if (m->nseq) v->tlen = (int32_t)(((float)(m->nseq - 1) * m->seql + m->lastl) * v->timesc * FS);
    v->vibd = m->vibd; v->kvib = m->vibdec > 0.0f ? decay_k(m->vibdec * v->timesc, -4.0f) : 1.0f; v->vph = 0.0f;
    v->noise = m->noise; v->kn = m->noise > 0.0f ? decay_k(m->noisedec * v->timesc, LN_1000) : 0.0f;
    v->cut = m->fc0; v->kcut = meme_ramp_k(m->fc0, m->fc1, (float)v->tlen * INV_FS);
    for (int k = 0; k < 4; k++) v->ph[k] = 0.0f;
    v->flt.ic1 = v->flt.ic2 = 0.0f;
    if (m->ftype) svf_set(&v->flt, m->fc0, m->ftype == 2 ? m->q : (m->q > 0.0f ? qdb(m->q) : 0.707f));
    v->reps = m->reps; v->rep_t = (int32_t)(m->gap * v->timesc * FS);
    v->amp = 0.0f;
    mv_seg(v);
    if (PI_(5, M_MODE) == 1 && step != -1) {            /* STUTTER: four short hits, half a step apart */
        int32_t sl = (int32_t)(sps * 0.5f);
        if (v->tlen > sl * 9 / 10) { v->tlen = sl * 9 / 10; v->reps = 1; }
        if (step >= 0) for (int k = 1; k < 4; k++) ev_add(sl * k, E_MEME, (uint8_t)idx, 0);
    }
}
static void meme_run(float *l, float *r, int a, int b)
{
    for (int n = 0; n < NMV; n++) {
        mv_t *v = &mv[n];
        if (!v->on) continue;
        const meme_t *m = &MEMES[v->def];
        float ts = v->timesc;
        int32_t ta = (int32_t)(m->att * ts * FS), th = (int32_t)((m->att + m->hold) * ts * FS);
        float k = m->drive > 0.0f ? m->drive : 0.0f, nk = k > 0.0f ? 1.0f / ftanh(k) : 1.0f;
        for (int i = a; i < b; i++) {
            if (v->t >= v->tseg) mv_seg(v);
            if ((v->cr++ & 15) == 0 && m->ftype) {
                if (m->fc1 != m->fc0) svf_set(&v->flt, v->cut, m->ftype == 2 ? m->q : (m->q > 0.0f ? qdb(m->q) : 0.707f));
            }
            v->f *= v->kf;
            v->cut *= v->kcut;
            float f = v->f;
            if (v->vibd > 0.0f) { v->vph += m->vibf * INV_FS; f *= 1.0f + fsin1(v->vph) * v->vibd; v->vibd *= v->kvib; }
            float x = 0.0f;
            if (m->wave != 9) {
                v->ph[0] = wrap1(v->ph[0] + f * INV_FS);
                x = wave(m->wave, v->ph[0]);
                for (int o = 1; o < m->nosc; o++) {
                    v->ph[o] = wrap1(v->ph[o] + f * m->ratio[o - 1] * INV_FS);
                    x += wave(m->wave, v->ph[o]);
                }
                if (m->nosc > 1) x *= 0.5f;
            }
            if (v->noise > 0.0005f) { x += frnd() * v->noise; v->noise *= v->kn; }
            if (m->ftype) x = svf_run(&v->flt, x, m->ftype == 1 ? 0 : m->ftype == 2 ? 1 : 2);
            /* amp: attack, hold, release (exp) or for a sequence: each note gated */
            float env;
            if (m->nseq) {
                int32_t into = v->t - (v->tseg - (int32_t)((v->si == m->nseq ? m->lastl : m->seql) * ts * FS));
                int32_t len = (int32_t)((v->si == m->nseq ? m->lastl : m->seql) * ts * FS);
                env = into < (int32_t)(m->att * ts * FS) + 1 ? (float)into / ((float)(m->att * ts * FS) + 1.0f)
                    : into > len * 8 / 10 ? 1.0f - (float)(into - len * 8 / 10) / ((float)len * 0.15f + 1.0f) : 1.0f;
                if (env < 0.0f) env = 0.0f;
            } else if (v->t < ta) env = (float)v->t / (float)(ta + 1);
            else if (v->t < th) env = 1.0f;
            else { float rr = (float)(v->t - th) / (m->rel * ts * FS + 1.0f); env = fexp(rr * LN_1000); }
            x *= env * m->amp;
            if (k > 0.0f) x = ftanh(k * x) * nk;
            l[i] += x; r[i] += x;
            v->t++;
            if (v->t >= v->tlen) {
                if (--v->reps > 0) {                    /* again, from the start */
                    int reps = v->reps; int32_t rt = v->rep_t;
                    float pitch = v->pitch, tsc = v->timesc;
                    v->t = 0; v->seg = 0; v->si = 0;
                    v->f = (m->nseq ? m->seq[0] : m->f[0]) * pitch; v->noise = m->noise; v->cut = m->fc0;
                    mv_seg(v); v->reps = reps; v->rep_t = rt; v->timesc = tsc;
                } else { v->on = 0; break; }
            }
        }
    }
}

/* ------------------------------------------------------------------ sequencer + render */
static int32_t step_in;                                  /* samples until the next step */
static float step_acc;
static int seq_step;                                     /* the step that plays next */
static float trk_g[NTR];
static float comp_env, crush_hold[2];
static uint32_t crush_cnt;
static inline int audible(int t)
{
    int any = 0;
    for (int i = 0; i < NTR; i++) any |= solo[i];
    return !mute[t] && (!any || solo[t]);
}
static int pick_pattern(void)
{
    if (!chain_on) return pat_next;
    for (int k = 1; k <= NPAT; k++) {
        int p = (pat_play + k) % NPAT;
        for (int t = 0; t < NTR; t++)
            for (int i = 0; i < NSTEP; i++)
                if (PAT[p].s[t][i]) return p;
    }
    return pat_play;
}
static void play_step(int s)
{
    if (s == 0) pat_play = (uint8_t)pick_pattern();
    const pat_t *p = &PAT[pat_play];
    if (p->s[T_KICK][s] && audible(T_KICK)) kick_trig(p->s[T_KICK][s] == 2);
    if (p->s[T_HAT][s] && audible(T_HAT)) hat_trig(p->s[T_HAT][s], s);
    if (p->s[T_BASS][s] && audible(T_BASS)) bass_trig(p->s[T_BASS][s]);
    if (p->s[T_ARP][s] && audible(T_ARP)) arp_trig(p->s[T_ARP][s]);
    if (p->s[T_LEAD][s] && audible(T_LEAD)) {
        lead_on(p->s[T_LEAD][s]);
        ev_add((int32_t)(V[4][L_LEN] * sps), E_LEAD_OFF, p->s[T_LEAD][s], 0);
    }
    if (p->s[T_MEME][s] && audible(T_MEME)) meme_trig(p->s[T_MEME][s], s);
    cur_step = s;
    step_count++;
}
static void voices_run(int a, int b)
{
    kick_run(TL[T_KICK], TR_[T_KICK], a, b);
    hats_run(TL[T_HAT], TR_[T_HAT], a, b);
    bass_run(TL[T_BASS], TR_[T_BASS], a, b);
    arp_run(TL[T_ARP], TR_[T_ARP], a, b);
    lead_run(TL[T_LEAD], TR_[T_LEAD], a, b);
    meme_run(TL[T_MEME], TR_[T_MEME], a, b);
    /* bass duck recovers (~70 ms) */
    if (duck_g < 1.0f) { duck_g = 1.0f - (1.0f - duck_g) * fexp(-(float)(b - a) / (0.07f * FS)); if (duck_g > 0.999f) duck_g = 1.0f; }
    lfo_pos += (float)(b - a) / sps;
    if (lfo_pos >= 64.0f) lfo_pos -= 64.0f;
}
static void take_requests(void)
{
    while (req_r != req_w) {
        req_t q = reqq[req_r % NREQ];
        __asm__ volatile("" ::: "memory");
        req_r++;
        switch (q.kind) {
        case R_LEAD_ON: lead_on(q.a); break;
        case R_LEAD_OFF: lead_off(q.a); break;
        case R_MEME: meme_trig(q.a, -1); break;
        case R_KICK: kick_trig(q.a); break;
        case R_HAT: hat_trig(q.a, -1); break;
        case R_BASS: { /* a live bass note: a free key's pitch */
            float f = mtof((float)q.a);
            bass_trig(1); bass.f = bass.ft = f; bass_lastf = f; break;
        }
        case R_ARP: arp_trig(q.a); break;
        case R_START: playing = 1; seq_step = 0; step_in = 0; step_acc = 0.0f; lfo_pos = 0.0f; step_count = 0; break;
        case R_STOP: playing = 0; cur_step = -1; lead_off(-1); break;
        case R_PANIC: lead_off(-1); for (int v = 0; v < NMV; v++) mv[v].on = 0; break;
        default: break;
        }
    }
}
static void render(int32_t *out, uint32_t n)
{
    if (n > BLK) n = BLK;
    float bpm = V[CTX_GLO][G_BPM];
    sps = FS * 60.0f / (bpm * 4.0f);
    for (int t = 0; t < NTR; t++)
        for (uint32_t i = 0; i < n; i++) TL[t][i] = TR_[t][i] = 0.0f;
    take_requests();
    int pos = 0;
    while (pos < (int)n) {
        int end = (int)n;
        if (playing && step_in < end) end = step_in < pos ? pos : step_in;
        for (int e = 0; e < nev; e++) if (evq[e].at < end) end = evq[e].at < pos ? pos : evq[e].at;
        if (end > pos) { voices_run(pos, end); pos = end; }
        /* what is due now */
        for (int e = 0; e < nev; e++)
            if (evq[e].at <= pos) {
                ev_t x = evq[e];
                evq[e] = evq[--nev]; e--;
                if (x.kind == E_HAT) hat_hit(1, 0.5f + 0.5f * (float)x.a / 255.0f + 0.5f / (float)x.b, -1);
                else if (x.kind == E_LEAD_OFF) lead_off(x.a);
                else if (x.kind == E_MEME) meme_trig(x.a, -2);
            }
        if (playing && step_in <= pos) {
            play_step(seq_step);
            float sw = V[CTX_GLO][G_SWING] * sps * 0.66f;
            int next = (seq_step + 1) % NSTEP;
            float iv = sps + ((next & 1) ? sw : -sw);
            if (seq_step == NSTEP - 1 && sw > 0.0f) iv = sps - sw;
            step_acc += iv;
            int32_t whole = (int32_t)step_acc;
            step_acc -= (float)whole;
            step_in = pos + whole;
            seq_step = next;
        }
    }
    if (playing) step_in -= (int32_t)n;
    for (int e = 0; e < nev; e++) evq[e].at -= (int32_t)n;

    /* mix: volume, pan, mute/solo (smooth), echo send */
    float gl[NTR], gr[NTR], snd[NTR], tg[NTR];
    for (int t = 0; t < NTR; t++) {
        float pan = V[CTX_MIX][8 + t];
        gl[t] = pan > 0.0f ? 1.0f - pan : 1.0f; gr[t] = pan < 0.0f ? 1.0f + pan : 1.0f;
        snd[t] = *pval(CTX_MIX, 16 + t);
        tg[t] = audible(t) ? *pval(CTX_MIX, t) : 0.0f;
    }
    int32_t edl = (int32_t)(sps * (float)(PI_(5, M_ETIME) + 1));
    if (edl > 32767) edl = 32767;
    if (edl < 1) edl = 1;
    float fb = V[5][M_FB];
    float dk = 0.5f + V[CTX_GLO][G_DRIVE] * 9.0f, dn = 1.0f / ftanh(dk);
    float cr = V[CTX_GLO][G_CRUSH];
    int bits = (int)(16.0f - cr * 13.0f + 0.5f), ds = 1 + (int)(cr * cr * 11.0f + 0.5f);
    float lv = (float)(1 << (bits - 1)), ilv = 1.0f / lv;
    float master = V[CTX_MIX][X_MASTER] * 0.8f;
    float ca = fexp(-1.0f / (0.003f * FS)), crl = fexp(-1.0f / (0.25f * FS));
    for (uint32_t i = 0; i < n; i++) {
        float l = 0.0f, r = 0.0f, es = 0.0f;
        for (int t = 0; t < NTR; t++) {
            trk_g[t] += (tg[t] - trk_g[t]) * 0.002f;
            float a = TL[t][i] * trk_g[t], b = TR_[t][i] * trk_g[t];
            l += a * gl[t]; r += b * gr[t];
            es += (a + b) * 0.5f * snd[t];
        }
        float e = (float)echo_buf[(echo_w - (uint32_t)edl) & 32767u] * (1.0f / 32767.0f);
        echo_buf[echo_w & 32767u] = (int16_t)(clampf(es + e * fb, -1.0f, 1.0f) * 32767.0f);
        echo_w++;
        l += e; r += e;
        /* drive, crusher */
        l = ftanh(dk * clampf(l, -1.5f, 1.5f)) * dn; r = ftanh(dk * clampf(r, -1.5f, 1.5f)) * dn;
        if (crush_cnt++ % (uint32_t)ds == 0) {
            crush_hold[0] = (float)(int)(l * lv + (l < 0 ? -0.5f : 0.5f)) * ilv;
            crush_hold[1] = (float)(int)(r * lv + (r < 0 ? -0.5f : 0.5f)) * ilv;
        }
        l = crush_hold[0]; r = crush_hold[1];
        /* compressor: -10 dB, 6:1 */
        float pk = fabsf_(l) > fabsf_(r) ? fabsf_(l) : fabsf_(r);
        comp_env = pk > comp_env ? pk + (comp_env - pk) * ca : pk + (comp_env - pk) * crl;
        float g = 1.0f;
        if (comp_env > 0.316f) g = (0.316f + (comp_env - 0.316f) / 6.0f) / comp_env;
        l *= g * master; r *= g * master;
        int32_t li = (int32_t)(clampf(l, -1.0f, 1.0f) * 30000.0f), ri = (int32_t)(clampf(r, -1.0f, 1.0f) * 30000.0f);
        out[2 * i] += li; out[2 * i + 1] += ri;
    }
}

/* ------------------------------------------------------------------ store */
#define STORE_MAGIC 0x4D454D31u                          /* "MEM1" */
typedef struct { uint32_t magic, version; pat_t pat[NPAT]; float v[NCTX][MAXP]; uint8_t chain; } store_t;
static store_t store;
static int store_load(void)
{
    if (!HB_HOST_HAS(H, bank_load)) return -1;
    if (H->bank_load(&store, sizeof store) != 0 || store.magic != STORE_MAGIC || store.version != 1) return -1;
    for (int p = 0; p < NPAT; p++) PAT[p] = store.pat[p];
    for (int c = 0; c < NCTX; c++)
        for (int i = 0; i < NDEF[c]; i++) {
            const pdef_t *d = &DEFS[c][i];
            if (d->actx >= 0 || !d->name) continue;
            float x = store.v[c][i];
            V[c][i] = (x >= d->lo && x <= d->hi) ? x : d->def;
        }
    chain_on = store.chain;
    return 0;
}
static int store_save(void)
{
    if (!HB_HOST_HAS(H, bank_save)) return -1;
    store.magic = STORE_MAGIC; store.version = 1;
    for (int p = 0; p < NPAT; p++) store.pat[p] = PAT[p];
    for (int c = 0; c < NCTX; c++) for (int i = 0; i < MAXP; i++) store.v[c][i] = V[c][i];
    store.chain = chain_on;
    return H->bank_save(&store, sizeof store);
}

/* ------------------------------------------------------------------ UI */
static int track = T_BASS, ctx = T_BASS, page, step_mode = 1, octave, meme_sel = 1, lead_cur;
static uint32_t dirty = 0xFFFFFFFFu;
enum { D_HEAD = 1, D_GRID = 2, D_PAR = 4, D_FOOT = 8, D_TITLE = 16, D_ALL = 31 };
static const char *msg; static uint32_t msg_until;
static int last_drawn_step = -2;
/* white keys -> step index; black keys -> 0..10 */
static const int8_t KEY_STEP[27] = {0, -1, 1, -1, 2, -1, 3, 4, -1, 5, -1, 6, 7, -1, 8, -1, 9, -1, 10, 11, -1, 12, -1, 13, 14, -1, 15};
static const int8_t KEY_BLACK[27] = {-1, 0, -1, 1, -1, 2, -1, -1, 3, -1, 4, -1, -1, 5, -1, 6, -1, 7, -1, -1, 8, -1, 9, -1, -1, 10, -1};
static const uint8_t STEP_KEY[16] = {0, 2, 4, 6, 7, 9, 11, 12, 14, 16, 18, 19, 21, 23, 24, 26};

static int npages(int c) { return (NDEF[c] + 3) / 4; }
static void flash_msg(const char *s) { msg = s; msg_until = H->ms() + 1200u; if (!msg_until) msg_until = 1; dirty |= D_FOOT; }

static void leds_update(void)
{
    static uint32_t shown = 0xFFFFFFFFu; static int shown_step = -2, shown_mode = -1, shown_play = -1, shown_rec = -1;
    uint32_t bits = 0;
    const uint8_t *s = PAT[pat_edit].s[track];
    for (int i = 0; i < NSTEP; i++) if (s[i]) bits |= 1u << i;
    int st = playing ? cur_step : -1;
    if (step_mode && (bits != shown || st != shown_step || shown_mode != 1)) {
        for (int i = 0; i < NSTEP; i++) {
            uint32_t on = (bits >> i) & 1u;
            if (i == st) on = !on;
            H->led(HB_LED_KEY(STEP_KEY[i]), on);
        }
        for (int k = 0; k < 27; k++) if (KEY_BLACK[k] >= 0) {
            int b = KEY_BLACK[k];
            H->led(HB_LED_KEY(k), b < 6 ? (mute[b] ? 0 : solo[b] ? 1 : 2) : b < 10 ? (b - 6 == pat_edit ? 1 : 2) : (chain_on ? 1 : 0));
        }
        shown = bits; shown_step = st; shown_mode = 1;
    } else if (!step_mode && shown_mode != 0) {
        for (int k = 0; k < 27; k++) H->led(HB_LED_KEY(k), 0);
        shown_mode = 0;
    }
    if (shown_play != playing) { H->led(HB_BTN_PLAY, playing); shown_play = playing; }
    if (shown_rec != rec) { H->led(HB_BTN_REC, rec); shown_rec = rec; }
    static int shown_seq = -1;
    if (shown_seq != step_mode) { H->led(HB_BTN_SEQ, step_mode); shown_seq = step_mode; }
}

static void set_track(int t) { track = t; ctx = t; page = 0; dirty |= D_ALL; }
static void turn_param(int k, int clicks)
{
    int i = page * 4 + k;
    if (i >= NDEF[ctx]) return;
    const pdef_t *d = pdef(ctx, i);
    if (!d->name) return;
    float *v = pval(ctx, i);
    int c = clicks; if (c > 1 || c < -1) c *= 3;
    float x = *v + d->step * (float)c;
    *v = clampf(x, d->lo, d->hi);
    dirty |= D_PAR | (ctx == CTX_GLO ? D_HEAD : 0);
}
static void randomise(void)
{
    uint8_t *a = PAT[pat_edit].s[track];
    const uint8_t *k = PAT[pat_edit].s[T_KICK];
    for (int i = 0; i < NSTEP; i++) {
        float r = frnd01(), r2 = frnd01();
        switch (track) {
        case T_KICK: a[i] = (uint8_t)(i % 4 == 0 ? (i == 0 ? 2 : 1) : (r < 0.12f ? 1 : 0)); break;
        case T_HAT: a[i] = (uint8_t)(i % 4 == 2 ? (r < 0.85f ? 2 : 1) : (r < 0.1f ? 3 : r2 < 0.45f ? 1 : 0)); break;
        case T_BASS: a[i] = (uint8_t)(k[i] ? 0 : (r < 0.8f ? (r2 < 0.65f ? 1 : r2 < 0.83f ? 2 : 3) : 0)); break;
        case T_ARP: a[i] = (uint8_t)(r < 0.3f ? 1 + (int)(r2 * 2.99f) : 0); break;
        case T_LEAD: { static const int pent[5] = {0, 3, 5, 7, 10};
            a[i] = (uint8_t)(r < 0.5f ? 57 + PI_(2, B_ROOT) + pent[(int)(r2 * 4.99f)] + (frnd01() < 0.3f ? 12 : 0) : 0); break; }
        default: a[i] = (uint8_t)(r < 0.2f ? 1 + (int)(r2 * ((float)NMEME - 0.01f)) : 0); break;
        }
    }
    dirty |= D_GRID;
}
static void step_tap(int i)
{
    uint8_t *a = &PAT[pat_edit].s[track][i];
    switch (track) {
    case T_KICK: *a = (uint8_t)((*a + 1) % 3); break;
    case T_MEME: *a = (uint8_t)(*a == meme_sel ? 0 : meme_sel); break;
    case T_LEAD:
        if (i == lead_cur && *a) *a = 0;
        else if (!*a) *a = (uint8_t)(57 + PI_(2, B_ROOT));
        lead_cur = i; break;
    default: *a = (uint8_t)((*a + 1) % 4); break;
    }
    dirty |= D_GRID;
}
static int key_note(int k) { return 53 + k + 12 * octave; }
static void live_key(int k, int down)
{
    int note = key_note(k), black = KEY_BLACK[k] >= 0;
    if (!down) { if (track == T_LEAD) post(R_LEAD_OFF, (uint8_t)note, 0); return; }
    switch (track) {
    case T_KICK: post(R_KICK, (uint8_t)black, 0); break;
    case T_HAT: post(R_HAT, (uint8_t)(black ? 2 : 1), 0); break;
    case T_BASS: post(R_BASS, (uint8_t)(note - 24), 0); break;
    case T_ARP: post(R_ARP, (uint8_t)(1 + k % 3), 0); break;
    case T_LEAD: post(R_LEAD_ON, (uint8_t)note, 0); break;
    default: { int m = KEY_STEP[k] >= 0 ? KEY_STEP[k] + 1 : 0; if (m >= 1 && m <= NMEME) { meme_sel = m; post(R_MEME, (uint8_t)m, 0); dirty |= D_FOOT; } }
    }
    if (rec) {
        uint8_t *s = PAT[pat_edit].s[track];
        int v = track == T_LEAD ? note : track == T_MEME ? meme_sel : track == T_HAT ? (black ? 2 : 1) : track == T_KICK ? (black ? 2 : 1) : 1;
        if (track == T_MEME && KEY_STEP[k] < 0) return;
        if (playing) { int at = cur_step < 0 ? 0 : cur_step; s[at] = (uint8_t)v; }
        else { s[lead_cur] = (uint8_t)v; lead_cur = (lead_cur + 1) % NSTEP; }
        dirty |= D_GRID;
    }
}
static void ui(uint32_t ev, int32_t arg)
{
    uint32_t kind = HB_EV_KIND(ev), id = HB_EV_ID(ev);
    static uint32_t held;                                /* buttons held */
    if (kind == HB_EV_BTN_DOWN) {
        held |= 1u << id;
        switch (id) {
        case HB_BTN_PLAY: post(playing ? R_STOP : R_START, 0, 0); dirty |= D_HEAD | D_GRID; break;
        case HB_BTN_REC: rec = !rec; dirty |= D_FOOT; break;
        case HB_BTN_SEQ: step_mode = !step_mode; dirty |= D_FOOT; break;
        case HB_BTN_EDIT: page = (page + 1) % npages(ctx); dirty |= D_PAR | D_TITLE; break;
        case HB_BTN_ENV: ctx = track; page = 0; dirty |= D_ALL; break;
        case HB_BTN_FX: ctx = CTX_MIX; page = 0; dirty |= D_ALL; break;
        case HB_BTN_GLO: ctx = CTX_GLO; page = 0; dirty |= D_ALL; break;
        case HB_BTN_LFO:                                 /* the track's LFO page, where it has one */
            ctx = track;
            page = track == T_BASS ? 3 : track == T_ARP ? 2 : track == T_LEAD ? 3 : 0;
            dirty |= D_ALL; break;
        case HB_BTN_ARP:
            if (held & (1u << HB_BTN_SEL)) { for (int i = 0; i < NSTEP; i++) PAT[pat_edit].s[track][i] = 0; flash_msg("SPUR GELOESCHT"); }
            else { randomise(); flash_msg("DICE!"); }
            dirty |= D_GRID; break;
        case HB_BTN_OCT_DOWN: if (octave > -2) octave--; flash_msg(octave == -2 ? "OKT -2" : octave == -1 ? "OKT -1" : octave == 0 ? "OKT 0" : "OKT +1"); break;
        case HB_BTN_OCT_UP: if (octave < 2) octave++; flash_msg(octave == 2 ? "OKT +2" : octave == 1 ? "OKT +1" : octave == 0 ? "OKT 0" : "OKT -1"); break;
        case HB_BTN_SAVE:
            if (playing) { flash_msg("ERST STOP, DANN SAVE"); break; }
            flash_msg(store_save() == 0 ? "GESPEICHERT" : "SPEICHERN FEHLGESCHLAGEN"); break;
        default: break;
        }
    } else if (kind == HB_EV_BTN_UP) {
        held &= ~(1u << id);
    } else if (kind == HB_EV_ENC) {
        switch (id) {
        case HB_ENC_SELECT: { int t = ((track + arg) % NTR + NTR) % NTR; set_track(t); break; }
        case HB_ENC_ALGORITHM: { int np = npages(ctx); page = ((page + arg) % np + np) % np; dirty |= D_PAR | D_TITLE; break; }
        case HB_ENC_KNOB1: case HB_ENC_KNOB2: case HB_ENC_KNOB3: case HB_ENC_KNOB4: turn_param((int)id - HB_ENC_KNOB1, arg); break;
        case HB_ENC_PRESETS:
            meme_sel = (((meme_sel - 1 + arg) % NMEME) + NMEME) % NMEME + 1;
            dirty |= D_FOOT;
            if (!playing) post(R_MEME, (uint8_t)meme_sel, 0);
            break;
        default: break;
        }
    } else if (kind == HB_EV_KEY_DOWN || kind == HB_EV_KEY_UP) {
        int k = (int)id, down = kind == HB_EV_KEY_DOWN;
        if (k > 26) return;
        if (step_mode) {
            if (!down) return;
            if (KEY_STEP[k] >= 0) {
                int i = KEY_STEP[k];
                if (rec && track == T_LEAD) { lead_cur = i; dirty |= D_GRID; return; }
                step_tap(i);
                uint8_t v = PAT[pat_edit].s[track][i];
                if (!playing && v) {                     /* hear it */
                    switch (track) {
                    case T_KICK: post(R_KICK, v == 2, 0); break;
                    case T_HAT: post(R_HAT, v, 0); break;
                    case T_MEME: post(R_MEME, v, 0); break;
                    case T_LEAD: post(R_LEAD_ON, v, 0); post(R_LEAD_OFF, v, 0); break;
                    default: break;
                    }
                }
            } else {
                int b = KEY_BLACK[k];
                if (b < 6) {
                    if (held & (1u << HB_BTN_SEL)) solo[b] = !solo[b]; else mute[b] = !mute[b];
                } else if (b < 10) { pat_edit = (uint8_t)(b - 6); pat_next = pat_edit; if (!playing) pat_play = pat_edit; }
                else chain_on = !chain_on;
                dirty |= D_ALL;
            }
        } else live_key(k, down);
    }
}

/* ------------------------------------------------------------------ drawing */
static void fmt_num(char *s, int v, int sign)
{
    char b[12]; int n = 0, j = 0;
    if (v < 0) { s[j++] = '-'; v = -v; }
    else if (sign && v > 0) s[j++] = '+';
    do { b[n++] = (char)('0' + v % 10); v /= 10; } while (v && n < 11);
    while (n) s[j++] = b[--n];
    s[j] = 0;
}
static void cat(char *d, const char *s) { while (*d) d++; while (*s) *d++ = *s++; *d = 0; }
static void fmt_val(char *s, const pdef_t *d, float v)
{
    char *p;
    s[0] = 0;
    switch (d->fmt) {
    case F_LIST: cat(s, d->list[(int)(v + 0.5f)]); return;
    case F_HZ: fmt_num(s, (int)(v + 0.5f), 0); cat(s, "Hz"); return;
    case F_MS: fmt_num(s, (int)(v * 1000.0f + 0.5f), 0); cat(s, "ms"); return;
    case F_S: { int c = (int)(v * 100.0f + 0.5f); fmt_num(s, c / 100, 0); p = s; while (*p) p++;
        *p++ = '.'; *p++ = (char)('0' + c / 10 % 10); *p++ = (char)('0' + c % 10); *p++ = 's'; *p = 0; return; }
    case F_PCT: fmt_num(s, (int)(v * 100.0f + (v < 0 ? -0.5f : 0.5f)), 0); cat(s, "%"); return;
    case F_PM: fmt_num(s, (int)(v * 100.0f + (v < 0 ? -0.5f : 0.5f)), 1); cat(s, "%"); return;
    case F_X: { int c = (int)(v * 10.0f + 0.5f); s[0] = 'x'; fmt_num(s + 1, c / 10, 0); p = s; while (*p) p++;
        *p++ = '.'; *p++ = (char)('0' + c % 10); *p = 0; return; }
    case F_Q: { int c = (int)(v * 10.0f + 0.5f); fmt_num(s, c / 10, 0); p = s; while (*p) p++;
        *p++ = '.'; *p++ = (char)('0' + c % 10); *p = 0; return; }
    case F_ST: fmt_num(s, (int)(v + (v < 0 ? -0.5f : 0.5f)), 1); return;
    default: fmt_num(s, (int)(v + 0.5f), 0); return;
    }
}

#define MC_BG 0x0000u
#define MC_TXT 0xFFFFu
#define MC_DIM HB_RGB(120, 120, 132)
#define MC_LINE HB_RGB(45, 48, 56)
#define MC_EMPTY HB_RGB(30, 32, 38)
#define MC_BEAT HB_RGB(48, 50, 60)

static int32_t tw(uint32_t big, const char *s)
{
    if (HB_HOST_HAS(H, text_w)) return H->text_w(big, s);
    int n = 0; while (s[n]) n++;
    return n * (big ? 16 : 8);
}
static void cv_right(int32_t xr, int32_t y, uint32_t big, const char *s, uint16_t c) { H->cv_text(xr - tw(big, s), y, big, s, c); }
static void cv_center(int32_t x, int32_t w, int32_t y, uint32_t big, const char *s, uint16_t c) { H->cv_text(x + (w - tw(big, s)) / 2, y, big, s, c); }

/* rows: head 0..25, title 28..63, grid 68..123, params 128..211, foot 214..239 */
static void draw_head(void)
{
    char s[40] = "MEMETEK  P", b[12];
    fmt_num(b, pat_edit + 1, 0); cat(s, b);
    if (chain_on) cat(s, " CHAIN");
    H->cv_begin(240, 26, MC_BG);
    H->cv_text(4, 4, 0, s, MC_TXT);
    s[0] = 0;
    cat(s, playing ? "> " : "|| ");
    fmt_num(b, (int)(V[CTX_GLO][G_BPM] + 0.5f), 0); cat(s, b); cat(s, " BPM");
    cv_right(236, 4, 0, s, playing ? HB_RGB(120, 255, 150) : MC_DIM);
    H->cv_rect(0, 25, 240, 1, MC_LINE);
    H->cv_blit(0, 0);
}
static void draw_title(void)
{
    char s[24], b[8];
    const char *nm = ctx == CTX_MIX ? "MIXER" : ctx == CTX_GLO ? "GLOBAL" : TNAME[track];
    H->cv_begin(240, 36, MC_BG);
    H->cv_text(4, 2, 1, nm, ctx < NTR ? TCOL[ctx] : MC_TXT);
    s[0] = 0; cat(s, "S"); fmt_num(b, page + 1, 0); cat(s, b); cat(s, "/"); fmt_num(b, npages(ctx), 0); cat(s, b);
    cv_right(236, 10, 0, s, MC_DIM);
    H->cv_blit(0, 28);
}
static void draw_grid(void)
{
    const uint8_t *a = PAT[pat_edit].s[track];
    uint16_t col = TCOL[track];
    int st = playing && pat_play == pat_edit ? cur_step : -1;
    static const char *const NN[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
    H->cv_begin(240, 56, MC_BG);
    for (int i = 0; i < NSTEP; i++) {
        int x = 4 + (i % 8) * 29, y = (i / 8) * 28;
        uint16_t c = a[i] ? col : (i % 4 == 0 ? MC_BEAT : MC_EMPTY);
        if (i == st) c = MC_TXT;
        H->cv_rect(x, y, 27, 25, c);
        if (a[i]) {
            char lb[4] = {0, 0, 0, 0};
            if (track == T_MEME && a[i] <= NMEME) { const char *t = MEMES[a[i] - 1].abbr; lb[0] = t[0]; lb[1] = t[1]; lb[2] = t[2]; }
            else if (track == T_LEAD) { const char *t = NN[a[i] % 12]; lb[0] = t[0]; lb[1] = t[1]; }
            else if (track == T_HAT) lb[0] = a[i] == 2 ? 'O' : a[i] == 3 ? 'R' : 'x';
            else if (track == T_KICK) lb[0] = a[i] == 2 ? '!' : 0;
            else lb[0] = "?158"[a[i] & 3];
            if (lb[0]) cv_center(x, 27, y + 4, 0, lb, 0x0000u);
        }
        if (track == T_LEAD && i == lead_cur && (rec || step_mode)) H->cv_rect(x, y + 25, 27, 2, TCOL[T_LEAD]);
    }
    H->cv_blit(0, 68);
    last_drawn_step = st;
}
static void draw_params(void)
{
    char b[24];
    H->cv_begin(240, 84, MC_BG);
    for (int k = 0; k < 4; k++) {
        int i = page * 4 + k;
        int x = 4 + (k % 2) * 120, y = 4 + (k / 2) * 40;
        if (i >= NDEF[ctx]) continue;
        const pdef_t *dn = &DEFS[ctx][i];
        if (!dn->name) continue;
        const pdef_t *d = pdef(ctx, i);
        float v = *pval(ctx, i);
        fmt_val(b, d, v);
        H->cv_text(x, y, 0, dn->name, MC_DIM);
        cv_right(x + 112, y + 16, 0, b, MC_TXT);
        float fr = clampf((v - d->lo) / (d->hi - d->lo), 0.0f, 1.0f);
        H->cv_rect(x, y + 34, 112, 3, MC_LINE);
        H->cv_rect(x, y + 34, (int32_t)(fr * 112.0f), 3, ctx < NTR ? TCOL[ctx] : HB_RGB(200, 200, 210));
        char kn[3] = {'K', (char)('1' + k), 0};
        H->cv_text(x, y + 16, 0, kn, MC_LINE);
    }
    H->cv_blit(0, 128);
}
static void draw_foot(void)
{
    char s[40];
    H->cv_begin(240, 26, MC_BG);
    H->cv_rect(0, 0, 240, 1, MC_LINE);
    for (int t = 0; t < NTR; t++) {
        char l[2] = {TLET[t], 0};
        uint16_t c = mute[t] ? HB_RGB(70, 70, 70) : solo[t] ? HB_RGB(255, 210, 62) : TCOL[t];
        if (t == track) H->cv_rect(2 + t * 16, 4, 15, 20, HB_RGB(64, 64, 76));
        cv_center(2 + t * 16, 15, 6, 0, l, c);
    }
    if (msg && (int32_t)(H->ms() - msg_until) < 0) cv_right(236, 6, 0, msg, HB_RGB(255, 210, 62));
    else {
        s[0] = 0;
        cat(s, step_mode ? "STEP " : "PLAY ");
        if (rec) cat(s, "REC ");
        cat(s, MEMES[meme_sel - 1].name);
        cv_right(236, 6, 0, s, rec ? HB_RGB(255, 90, 90) : MC_DIM);
    }
    H->cv_blit(0, 214);
}
static void draw(uint32_t full)
{
    static uint32_t msg_was;
    static int was_playing = -1;
    if (full) { dirty = 0xFFFFFFFFu; H->fill(0, 0, 240, 240, MC_BG); }
    if (msg && msg_was != msg_until && (int32_t)(H->ms() - msg_until) >= 0) { msg_was = msg_until; dirty |= D_FOOT; }
    if (was_playing != playing) { was_playing = playing; dirty |= D_HEAD | D_GRID; }
    int st = playing && pat_play == pat_edit ? cur_step : -1;
    if (st != last_drawn_step) dirty |= D_GRID;
    if (dirty & D_HEAD) draw_head();
    if (dirty & D_TITLE) draw_title();
    if (dirty & D_GRID) draw_grid();
    if (dirty & D_PAR) draw_params();
    if (dirty & D_FOOT) draw_foot();
    dirty = 0;
    leds_update();
}

/* ------------------------------------------------------------------ module */
static void note(uint32_t ch, uint32_t key, uint32_t vel)
{
    (void)ch;
    if (key > 127u) return;
    if (vel) post(R_LEAD_ON, (uint8_t)key, 0); else post(R_LEAD_OFF, (uint8_t)key, 0);
}
static void cc(uint32_t ch, uint32_t num, uint32_t val) { (void)ch; (void)val; if (num == 123u) post(R_PANIC, 0, 0); }
static int init(const hb_host_t *h)
{
    H = h;
    sin_init();
    for (int i = 0; i < 64; i++) NOISE64[i] = (rnd32() & 1u) ? 1.0f : -1.0f;
    for (int c = 0; c < NCTX; c++) params_default(c);
    patterns_default();
    store_load();
    for (int t = 0; t < NTR; t++) trk_g[t] = 0.0f;
    dirty = 0xFFFFFFFFu;
    H->log("memetek: ready");
    return 0;
}
static void release(void)
{
    playing = 0; kick.on = 0; bass.on = 0; arp.on = 0; lead.on = 0;
    for (int v = 0; v < NHAT; v++) hats[v].on = 0;
    for (int v = 0; v < NMV; v++) mv[v].on = 0;
}
static const hb_param_t PARAMS[] = {{"BPM", 60, 200, 160}};
HB_DESC = {HB_ABI, "MEMETEK", init, release, note, cc, render, ui, draw, PARAMS, 1, HB_F_SCREEN | HB_F_PANEL, 0, 0};
