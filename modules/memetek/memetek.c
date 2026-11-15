/* SPDX-License-Identifier: GPL-3.0-only */
/* Memetek FM1: a groovebox for 8-bit memetek on the M-VAVE FM-1, as an fm1-dock module.
 * Six tracks (TEK-KICK, HATS, 8-BIT BASS, CHIP ARP, 8-BIT LEAD, MEME FX), a 16-step sequencer with
 * four patterns, pages of parameters on the four knobs, a mixer, an echo, a bitcrusher.
 *
 * Panel (HB_F_PANEL):
 *   PLAY        start / stop            SEQ   STEP mode (white keys = steps) <-> PLAY mode (keys play)
 *   REC         record (running: in time; stopped: step input)
 *   SELECT      the track               ALGORITHM / EDIT   the page
 *   KNOB1..4    the page's parameters   PRESETS   the meme sound
 *   ENV         track pages             FX    mixer           GLO   tempo, swing, crush, drive
 *   SEL tapped  tap tempo               EDIT  copy the pattern into the next one
 *   HOME tapped back to the track page, STEP mode
 *   OCT- / OCT+ STEP mode: shift the track one step; PLAY mode: the octave
 *   MEME in PLAY mode: white keys the 16 sounds of the bank, OP1..OP6 bank A..F, PIT..the last key pitch -12 -5 0 +7 +12
 *   ARP         dice: randomise the track   SEL + ARP   clear the track
 *   SAVE        keep patterns and sounds in flash
 *   STEP mode, the upper row: OP1..OP6 mute KICK..MEME (SEL held: solo), PIT GLO MONO POLY pattern 1..4
 *               (first press: show and edit it, the playing one goes on; second press: activate it),
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
static const char *const L_NOTES[] = {"A", "A#", "B", "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#"};
static const char *const L_LTB[] = {"CUTOFF", "RESO", "PITCH", "F-ENV", "DECAY", "SUB", "VOL", "TIMBRE"};
static const char *const L_WAVES[] = {"SQUARE", "PULSE", "SAW", "TRI", "SUPERSAW", "FM", "PWM", "SYNC", "FOLD"};
static const char *const L_FILT[] = {"LOWPASS", "BANDPASS", "HIGHPASS"};
static const char *const L_RATE[] = {"4BAR", "2BAR", "1BAR", "1/2", "1/4", "1/8", "1/16", "1/32"};
static const char *const L_SHAPE[] = {"SINE", "TRI", "SAW", "SQUARE", "S&H"};
static const char *const L_ARATE[] = {"2/STP", "3/STP", "4/STP", "6/STP"};
static const char *const L_DIR[] = {"UP", "DOWN", "PING", "RND"};
static const char *const L_CHORD[] = {"MOLL", "DUR", "SUS", "POWER"};
static const char *const L_LTC[] = {"PITCH", "VOL", "TONE", "PAN", "DECAY", "OKTAVE", "TIMBRE"};
static const char *const L_LTL[] = {"PITCH", "VOL", "TONE", "PAN", "TIMBRE"};
static const char *const L_LWAVE[] = {"SQUARE", "PULSE25", "PULSE12", "TRI", "SAW", "NOISE", "SUPERSAW", "FM", "PWM", "SYNC", "FOLD"};
static const char *const L_LARP[] = {"AUS", "OKTAVE", "DUR", "MOLL"};
static const char *const L_MMODE[] = {"NORMAL", "STUTTER"};
static const char *const L_LTM[] = {"PITCH", "VOL", "TONE", "PAN", "ECHO", "LAENGE"};
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
enum { B_CUT, B_RES, B_FENV, B_DECAY, B_WAVE, B_TIMB, B_SUB, B_VOL, B_ROOT, B_OCT, B_DUCK, B_DRIVE,
       B_GLIDE, B_TENV, B_WIDTH, B_FTYPE, B_L1T, B_L1R, B_L1D, B_L1S, B_L2T, B_L2R, B_L2D, B_L2S };
static const pdef_t D_BASS[] = {
    P("CUTOFF", 100, 5000, 10, 2400, F_HZ), P("RESO", 0.5f, 20, 0.1f, 6, F_Q), P("F-ENV", 0, 1, 0.01f, 0.8f, F_PCT), P("DECAY", 0.2f, 1.5f, 0.05f, 0.9f, F_X),
    PL("WAVE", 8, 0, L_WAVES), P("TIMBRE", 0, 1, 0.01f, 0.5f, F_PCT), P("SUB", 0, 1, 0.01f, 0.5f, F_PCT), P("VOL", 0, 1, 0.01f, 0.5f, F_PCT),
    PL("ROOT", 11, 5, L_NOTES), P("OCT", -1, 1, 1, 0, F_ST), P("DUCK", 0, 1, 0.01f, 0, F_PCT), P("DRIVE", 0, 1, 0.01f, 0, F_PCT),
    P("GLIDE", 0, 0.2f, 0.005f, 0, F_MS), P("T-ENV", -1, 1, 0.01f, 0, F_PM), P("WIDTH", 0, 1, 0.01f, 0.5f, F_PCT), PL("FILTER", 2, 0, L_FILT),
    PL("LFO1 ZIEL", 7, 0, L_LTB), PL("RATE", 7, 4, L_RATE), P("DEPTH", -1, 1, 0.01f, 0, F_PM), PL("FORM", 4, 0, L_SHAPE),
    PL("LFO2 ZIEL", 7, 2, L_LTB), PL("RATE", 7, 6, L_RATE), P("DEPTH", -1, 1, 0.01f, 0, F_PM), PL("FORM", 4, 0, L_SHAPE)};
enum { A_RATE, A_DIR, A_WAVE, A_VOL, A_OCT, A_DECAY, A_VIB, A_CHORD, A_LT, A_LR, A_LD, A_LS, A_TIMB, A_TENV, A_WIDTH };
static const pdef_t D_ARP[] = {
    PL("RATE", 3, 2, L_ARATE), PL("RICHTG", 3, 0, L_DIR), PL("WAVE", 8, 0, L_WAVES), P("VOL", 0, 0.6f, 0.01f, 0.22f, F_PCT),
    P("OCT", -1, 2, 1, 0, F_ST), P("DECAY", 0.2f, 1, 0.01f, 0.98f, F_PCT), P("VIBRATO", 0, 1, 0.01f, 0, F_PCT), PL("AKKORD", 3, 0, L_CHORD),
    PL("LFO ZIEL", 6, 0, L_LTC), PL("RATE", 7, 5, L_RATE), P("DEPTH", -1, 1, 0.01f, 0, F_PM), PL("FORM", 4, 0, L_SHAPE),
    P("TIMBRE", 0, 1, 0.01f, 0.5f, F_PCT), P("T-ENV", -1, 1, 0.01f, 0, F_PM), P("WIDTH", 0, 1, 0.01f, 0.5f, F_PCT), PN};
enum { L_WAVE, L_LEN, L_VIB, L_VOL, L_OCT, L_GLIDE, L_SWEEP, L_DET, L_ATT, L_DEC, L_SUS, L_ARP, L_LT, L_LR, L_LD, L_LS, L_TIMB, L_TENV, L_WIDTH };
static const pdef_t D_LEAD[] = {
    PL("WAVE", 10, 0, L_LWAVE), P("LAENGE", 0.25f, 4, 0.25f, 1, F_X), P("VIBRATO", 0, 1, 0.01f, 0, F_PCT), P("VOL", 0, 1, 0.01f, 0.35f, F_PCT),
    P("OCT", -2, 2, 1, 0, F_ST), P("GLIDE", 0, 0.3f, 0.005f, 0, F_MS), P("SWEEP", -12, 12, 1, 0, F_ST), P("DETUNE", 0, 1, 0.01f, 0, F_PCT),
    P("ATTACK", 0, 0.3f, 0.005f, 0, F_MS), P("DECAY", 0.05f, 2, 0.01f, 0.4f, F_S), P("SUSTAIN", 0, 1, 0.01f, 0.6f, F_PCT), PL("ARP", 3, 0, L_LARP),
    PL("LFO ZIEL", 4, 0, L_LTL), PL("RATE", 7, 4, L_RATE), P("DEPTH", -1, 1, 0.01f, 0, F_PM), PL("FORM", 4, 0, L_SHAPE),
    P("TIMBRE", 0, 1, 0.01f, 0.5f, F_PCT), P("T-ENV", -1, 1, 0.01f, 0, F_PM), P("WIDTH", 0, 1, 0.01f, 0.5f, F_PCT), PN};
enum { M_PITCH, M_LEN, M_MODE, M_VOL, M_RND, M_ECHO, M_ETIME, M_FB, M_LT, M_LR, M_LD, M_LS };
static const pdef_t D_MEME[] = {
    P("PITCH", -12, 12, 1, 0, F_ST), P("LAENGE", 0.1f, 1, 0.01f, 1, F_PCT), PL("MODE", 1, 0, L_MMODE), P("VOL", 0, 1, 0.01f, 0.5f, F_PCT),
    P("RND-PITCH", 0, 12, 1, 0, F_INT), P("ECHO", 0, 1, 0.01f, 0, F_PCT), PL("E-ZEIT", 3, 1, L_ETIME), P("FEEDBK", 0, 0.9f, 0.01f, 0.4f, F_PCT),
    PL("LFO ZIEL", 5, 0, L_LTM), PL("RATE", 7, 5, L_RATE), P("DEPTH", -1, 1, 0.01f, 0, F_PM), PL("FORM", 4, 0, L_SHAPE)};
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
static const uint8_t NDEF[NCTX] = {8, 12, 24, 16, 20, 12, 24, 4};
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

/* ------------------------------------------------------------------ memes */
#include "memes.inc"

/* ------------------------------------------------------------------ patterns */
typedef struct { uint8_t s[NTR][NSTEP]; } pat_t;
static pat_t PAT[NPAT];                                  /* what you see and edit */
static pat_t LIVE[NPAT];                                 /* what plays: an edit reaches it when its pattern is activated */
static uint8_t pending[NPAT];                            /* edits not yet activated */
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
    PAT[0].s[T_MEME][14] = (uint8_t)meme_id("HORN");
    PAT[1].s[T_MEME][0] = (uint8_t)meme_id("BRUH"); PAT[1].s[T_MEME][6] = (uint8_t)meme_id("BOING"); PAT[1].s[T_MEME][10] = (uint8_t)meme_id("QUACK");
    PAT[1].s[T_MEME][12] = (uint8_t)meme_id("FART"); PAT[1].s[T_MEME][14] = (uint8_t)meme_id("HORN x3");
    PAT[1].s[T_LEAD][8] = 74; PAT[1].s[T_LEAD][10] = 72; PAT[1].s[T_LEAD][12] = 69;
    PAT[2].s[T_MEME][3] = (uint8_t)meme_id("JUMP"); PAT[2].s[T_MEME][7] = (uint8_t)meme_id("COIN");
    PAT[2].s[T_MEME][11] = (uint8_t)meme_id("JUMP"); PAT[2].s[T_MEME][15] = (uint8_t)meme_id("1UP");
    PAT[3].s[T_MEME][0] = (uint8_t)meme_id("DROP"); PAT[3].s[T_MEME][4] = (uint8_t)meme_id("SPRING");
    PAT[3].s[T_MEME][8] = (uint8_t)meme_id("RIMSHOT"); PAT[3].s[T_MEME][12] = (uint8_t)meme_id("SAD");
}

/* ------------------------------------------------------------------ shared state (main loop <-> audio) */
static volatile uint8_t playing, rec, chain_on;
static volatile uint8_t pat_edit, pat_play, pat_next;
static volatile uint8_t mute[NTR], solo[NTR];
static volatile int live_shift;                        /* MEME live pitch (semitones), the upper row in PLAY mode */
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

/* ------------------------------------------------------------------ the synth oscillator
 * The classic 8-bit waves and more synthesis, one call per sample, stereo out:
 *   SQUARE PULSE SAW TRI   naive, aliasing on purpose (8-bit)
 *   SUPERSAW               seven detuned saws (TIMBRE: the detune), spread over the stereo field (WIDTH)
 *   FM                     two-operator phase modulation (TIMBRE: the index), the modulator an octave up
 *   PWM                    a pulse whose width an internal LFO moves (TIMBRE: the depth)
 *   SYNC                   a hard-synced saw (TIMBRE: the slave's pitch)
 *   FOLD                   a sine through a wavefolder (TIMBRE: the drive)
 *   PULSE12, NOISE         the lead's thin pulse and the 8-bit noise channel */
enum { O_SQUARE, O_PULSE, O_SAW, O_TRI, O_SUPER, O_FM, O_PWM, O_SYNC, O_FOLD, O_PULSE12, O_NOISE };
static float NOISE64[64];
typedef struct { float ph[7], mph, pwm; } osc_t;
static const float SSD[7] = {-0.11002313f, -0.06288439f, -0.01952356f, 0.0f, 0.01991221f, 0.06216538f, 0.10745242f};
static void osc_reset(osc_t *o, int free_run)
{
    if (free_run) for (int k = 0; k < 7; k++) o->ph[k] = frnd01();    /* a supersaw starts unaligned: no flam */
    else for (int k = 0; k < 7; k++) o->ph[k] = 0.0f;
    o->mph = 0.0f;
}
static inline void osc_run(osc_t *o, int m, float f, float tim, float width, float *ol, float *orr)
{
    float x;
    switch (m) {
    case O_SUPER: {
        float d = 0.04f + tim * 0.96f, sl = 0.0f, sr = 0.0f, wl = 0.5f + 0.5f * width, wr = 0.5f - 0.5f * width;
        for (int k = 0; k < 7; k++) {
            o->ph[k] = wrap1(o->ph[k] + f * (1.0f + SSD[k] * d) * INV_FS);
            float s = 2.0f * o->ph[k] - 1.0f;
            if (k == 3) { sl += s; sr += s; }
            else if (k & 1) { sl += 0.6f * s * wl; sr += 0.6f * s * wr; }
            else { sl += 0.6f * s * wr; sr += 0.6f * s * wl; }
        }
        *ol = sl * 0.42f; *orr = sr * 0.42f;
        return;
    }
    case O_FM:
        o->ph[0] = wrap1(o->ph[0] + f * INV_FS);
        o->mph = wrap1(o->mph + 2.0f * f * INV_FS);
        x = fsin1(o->ph[0] + tim * 0.9f * fsin1(o->mph));
        break;
    case O_PWM: {
        o->ph[0] = wrap1(o->ph[0] + f * INV_FS);
        o->pwm += 0.7f * INV_FS;
        if (o->pwm >= 1.0f) o->pwm -= 1.0f;
        float w = 0.5f + 0.45f * tim * fsin1(o->pwm);
        x = o->ph[0] < w ? 1.0f : -1.0f;
        break;
    }
    case O_SYNC: {
        float p = o->ph[0] + f * INV_FS;
        if (p >= 1.0f) { p -= 1.0f; o->ph[1] = 0.0f; }
        o->ph[0] = p;
        o->ph[1] = wrap1(o->ph[1] + f * (1.0f + tim * 7.0f) * INV_FS);
        x = 2.0f * o->ph[1] - 1.0f;
        break;
    }
    case O_FOLD:
        o->ph[0] = wrap1(o->ph[0] + f * INV_FS);
        x = fsin1(fsin1(o->ph[0]) * (1.0f + tim * 6.0f) * 0.25f);
        break;
    case O_NOISE:
        o->ph[0] = wrap1(o->ph[0] + f * INV_FS);
        x = NOISE64[(int)(o->ph[0] * 64.0f) & 63];
        break;
    default:
        o->ph[0] = wrap1(o->ph[0] + f * INV_FS);
        x = wave(m == O_PULSE12 ? 4 : m, o->ph[0]);   /* wave(): 0 square 1 pulse25 2 saw 3 tri 4 pulse12 */
        break;
    }
    *ol = x; *orr = x;
}

/* ------------------------------------------------------------------ BASS */
static const int IV[4] = {0, 0, 7, 12};
static struct { int on, att, cr; float ph, phs, f, ft, kg, amp, peak, ka, cut, kc, decm, fenv, tenv, ktenv; osc_t o; svf_t lp, lp2; } bass;
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
    if (!bass.on) osc_reset(&bass.o, PI_(2, B_WAVE) == O_SUPER);
    bass.on = 1; bass.att = (int)(0.003f * FS); bass.peak = 0.8f; bass.amp = 0.0f;
    bass.ka = decay_k(ln * 0.95f, LN_1000);
    bass.fenv = clampf(V[2][B_FENV] + mfe, 0.0f, 1.0f);
    bass.cut = 1.0f;                                     /* the envelope, as a factor of CUTOFF: 1 -> 1 - fenv*.92 */
    float endf = 1.0f - bass.fenv * 0.92f;
    if (endf < 0.02f) endf = 0.02f;
    float upd = ln * 0.8f * FS / 16.0f;
    if (upd < 1.0f) upd = 1.0f;
    bass.kc = fexp2(flog2(endf) / upd);
    bass.tenv = 1.0f; bass.ktenv = fexp2(flog2(0.05f) / (upd * 0.5f > 1.0f ? upd * 0.5f : 1.0f));
    bass.cr = 0;
}
static void bass_run(float *l, float *r, int a, int b)
{
    if (!bass.on) return;
    int w = PI_(2, B_WAVE), fm = PI_(2, B_FTYPE);
    static float sub = 0, vol = 1, pm = 1, tim = 0.5f;
    float k = 0.5f + V[2][B_DRIVE] * 10.0f, nk = 1.0f / ftanh(k), width = V[2][B_WIDTH];
    for (int i = a; i < b; i++) {
        if ((bass.cr++ & 15) == 0) {
            int t1, t2;
            float pos = lfo_pos + (float)(i - a) / sps;
            float m1 = lfo_val(2, B_L1T, B_L1R, B_L1D, B_L1S, pos, 1000, &t1);
            float m2 = lfo_val(2, B_L2T, B_L2R, B_L2D, B_L2S, pos, 2000, &t2);
            float mc = (t1 == 0 ? m1 : 0) + (t2 == 0 ? m2 : 0), mr = (t1 == 1 ? m1 : 0) + (t2 == 1 ? m2 : 0);
            float mp = (t1 == 2 ? m1 : 0) + (t2 == 2 ? m2 : 0), ms = (t1 == 5 ? m1 : 0) + (t2 == 5 ? m2 : 0);
            float mv = (t1 == 6 ? m1 : 0) + (t2 == 6 ? m2 : 0), mt = (t1 == 7 ? m1 : 0) + (t2 == 7 ? m2 : 0);
            float fc = clampf(V[2][B_CUT] * fexp2(3.0f * mc), 60.0f, 12000.0f) * bass.cut;
            svf_set(&bass.lp, fc < 50.0f ? 50.0f : fc, qdb(clampf(V[2][B_RES] + mr * 10.0f, 0.5f, 25.0f)));
            bass.lp2.g = bass.lp.g; bass.lp2.k = bass.lp.k; bass.lp2.a1 = bass.lp.a1; bass.lp2.a2 = bass.lp.a2; bass.lp2.a3 = bass.lp.a3;
            pm = mp != 0.0f ? semis(mp * 12.0f) : 1.0f;
            sub = clampf(V[2][B_SUB] + ms, 0.0f, 1.0f);
            vol = clampf(1.0f + mv, 0.0f, 2.0f);
            tim = clampf(V[2][B_TIMB] + V[2][B_TENV] * bass.tenv + mt, 0.0f, 1.0f);
            bass.tenv *= bass.ktenv;
            bass.cut *= bass.kc;
            float floorc = 1.0f - bass.fenv * 0.92f;
            if (bass.cut < floorc) bass.cut = floorc;
        }
        if (bass.kg > 0.0f) bass.f = bass.ft + (bass.f - bass.ft) * bass.kg;
        float f = bass.f * pm, xl, xr;
        osc_run(&bass.o, w, f, tim, width, &xl, &xr);
        bass.phs = wrap1(bass.phs + f * 0.5f * INV_FS);
        float s = wave(3, bass.phs) * sub;
        xl = svf_run(&bass.lp, xl, fm) + s;
        xr = (w == O_SUPER ? svf_run(&bass.lp2, xr, fm) : xl - s) + s;
        if (bass.att > 0) { bass.amp += bass.peak / (0.003f * FS); bass.att--; } else bass.amp *= bass.ka;
        float g = bass.amp * vol;
        xl = ftanh(k * xl * g) * nk * duck_g;
        xr = ftanh(k * xr * g) * nk * duck_g;
        l[i] += xl; r[i] += xr;
    }
    if (!bass.att && bass.amp < 0.0005f) bass.on = 0;
}

/* ------------------------------------------------------------------ CHIP ARP */
static const int CHORDS[4][4] = {{0, 3, 7, 12}, {0, 4, 7, 12}, {0, 5, 7, 12}, {0, 7, 12, 19}};
static const int CHL[4] = {0, 0, 5, 7};
static struct { int on, n, idx, sub, cnt, cr; float f[6], amp, ka, vph, tenv, ktenv; osc_t o; svf_t lp, lp2; } arp;
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
    if (!arp.on) osc_reset(&arp.o, PI_(3, A_WAVE) == O_SUPER);
    arp.on = 1; arp.n = n; arp.idx = 0; arp.sub = (int)(sps / (float)n); arp.cnt = 0; arp.cr = 0;
    arp.amp = 0.9f; arp.ka = decay_k(ln * 0.98f, LN_1000);
    arp.tenv = 1.0f; arp.ktenv = decay_k(ln * 0.3f / 16.0f, -1.0f);
    arp.lp.ic1 = arp.lp.ic2 = arp.lp2.ic1 = arp.lp2.ic2 = 0.0f;
}
static void arp_run(float *l, float *r, int a, int b)
{
    if (!arp.on) return;
    int w = PI_(3, A_WAVE);
    static float pm = 1, vm = 1, gl = 1, gr = 1, tim = 0.5f;
    static int tone;
    float width = V[3][A_WIDTH];
    for (int i = a; i < b; i++) {
        if ((arp.cr++ & 15) == 0) {
            int tg;
            float v = lfo_val(3, A_LT, A_LR, A_LD, A_LS, lfo_pos + (float)(i - a) / sps, 555, &tg);
            int on = V[3][A_LD] != 0.0f;
            pm = (on && tg == 0) ? semis(v * 12.0f) : 1.0f;
            if (V[3][A_VIB] > 0.0f) pm *= semis(fsin1(arp.vph) * V[3][A_VIB] * 0.6f);
            vm = (on && tg == 1) ? clampf(1.0f + v, 0.0f, 2.0f) : 1.0f;
            tone = on && tg == 2;
            if (tone) {
                svf_set(&arp.lp, clampf(2500.0f * fexp2(v * 2.3f), 200.0f, 16000.0f), qdb(2.0f));
                arp.lp2.g = arp.lp.g; arp.lp2.k = arp.lp.k; arp.lp2.a1 = arp.lp.a1; arp.lp2.a2 = arp.lp.a2; arp.lp2.a3 = arp.lp.a3;
            }
            float p = (on && tg == 3) ? clampf(v, -1.0f, 1.0f) : 0.0f;
            gl = p > 0 ? 1.0f - p : 1.0f; gr = p < 0 ? 1.0f + p : 1.0f;
            tim = clampf(V[3][A_TIMB] + V[3][A_TENV] * arp.tenv + ((on && tg == 6) ? v : 0.0f), 0.0f, 1.0f);
            arp.tenv *= arp.ktenv;
        }
        arp.vph += 6.0f * INV_FS;
        if (++arp.cnt >= arp.sub && arp.idx < arp.n - 1) { arp.cnt = 0; arp.idx++; }
        float xl, xr;
        osc_run(&arp.o, w, arp.f[arp.idx] * pm, tim, width, &xl, &xr);
        float g = arp.amp * vm;
        xl *= g; xr *= g;
        if (tone) { xl = svf_run(&arp.lp, xl, 0); xr = svf_run(&arp.lp2, xr, 0); }
        arp.amp *= arp.ka;
        l[i] += xl * gl; r[i] += xr * gr;
    }
    if (arp.amp < 0.0005f) arp.on = 0;
}

/* ------------------------------------------------------------------ 8-BIT LEAD */
static const int LARP[4][3] = {{0, 0, 0}, {0, 12, 12}, {0, 4, 7}, {0, 3, 7}};
static const int LARPN[4] = {1, 2, 3, 3};
static const uint8_t LWMAP[11] = {O_SQUARE, O_PULSE, O_PULSE12, O_TRI, O_SAW, O_NOISE, O_SUPER, O_FM, O_PWM, O_SYNC, O_FOLD};
static struct { int on, gate, note, arpi, arpc, cr, stage; float f, ft, kg, amp, vph, kdec, krel, tenv, ktenv; osc_t o, o2; svf_t lp, lp2; } lead;
static float lead_lastf;
static void lead_on(int note)
{
    float f = mtof((float)(note + 12 * PI_(4, L_OCT)));
    if (V[4][L_GLIDE] > 0.0f && lead_lastf > 0.0f && lead.gate) { lead.f = lead_lastf; lead.kg = decay_k(V[4][L_GLIDE] * 0.33f, -1.0f); }
    else if (V[4][L_SWEEP] != 0.0f) { lead.f = f * semis(V[4][L_SWEEP]); lead.kg = decay_k(0.02f, -1.0f); }
    else { lead.f = f; lead.kg = 0.0f; }
    lead.ft = f; lead_lastf = f;
    if (!lead.on) { int m = LWMAP[PI_(4, L_WAVE)]; osc_reset(&lead.o, m == O_SUPER); osc_reset(&lead.o2, 1); }
    lead.on = 1; lead.gate = 1; lead.note = note; lead.stage = 0;
    if (!(V[4][L_GLIDE] > 0.0f)) lead.amp = 0.0f;
    lead.kdec = decay_k(V[4][L_DEC] * 0.33f, -1.0f);
    lead.krel = decay_k(0.02f, -1.0f);
    lead.tenv = 1.0f; lead.ktenv = decay_k(V[4][L_DEC] * 0.3f / 16.0f, -1.0f);
    lead.arpi = 0; lead.arpc = 0;
}
static void lead_off(int note)
{
    if (lead.gate && (note < 0 || note == lead.note)) lead.gate = 0;
}
static void lead_run(float *l, float *r, int a, int b)
{
    if (!lead.on) return;
    int m = LWMAP[PI_(4, L_WAVE)], arpm = PI_(4, L_ARP);
    float det = V[4][L_DET], att = V[4][L_ATT], sus = V[4][L_SUS], width = V[4][L_WIDTH];
    static float pm = 1, vm = 1, gl = 1, gr = 1, tim = 0.5f;
    static int tone;
    int arps = (int)(sps * 0.25f);
    int dual = det > 0.0f && m != O_SUPER;
    for (int i = a; i < b; i++) {
        if ((lead.cr++ & 15) == 0) {
            int tg;
            float v = lfo_val(4, L_LT, L_LR, L_LD, L_LS, lfo_pos + (float)(i - a) / sps, 999, &tg);
            int on = V[4][L_LD] != 0.0f;
            pm = (on && tg == 0) ? semis(v * 12.0f) : 1.0f;
            if (V[4][L_VIB] > 0.0f) pm *= semis(fsin1(lead.vph) * V[4][L_VIB] * 0.45f);
            vm = (on && tg == 1) ? clampf(1.0f + v, 0.0f, 2.0f) : 1.0f;
            tone = on && tg == 2;
            if (tone) {
                svf_set(&lead.lp, clampf(2500.0f * fexp2(v * 2.3f), 200.0f, 16000.0f), qdb(2.0f));
                lead.lp2.g = lead.lp.g; lead.lp2.k = lead.lp.k; lead.lp2.a1 = lead.lp.a1; lead.lp2.a2 = lead.lp.a2; lead.lp2.a3 = lead.lp.a3;
            }
            float p = (on && tg == 3) ? clampf(v, -1.0f, 1.0f) : 0.0f;
            gl = p > 0 ? 1.0f - p : 1.0f; gr = p < 0 ? 1.0f + p : 1.0f;
            tim = clampf(V[4][L_TIMB] + V[4][L_TENV] * lead.tenv + ((on && tg == 4) ? v : 0.0f), 0.0f, 1.0f);
            lead.tenv *= lead.ktenv;
        }
        lead.vph += 6.0f * INV_FS;
        if (lead.kg > 0.0f) lead.f = lead.ft + (lead.f - lead.ft) * lead.kg;
        float f = lead.f * pm;
        if (arpm) {
            if (++lead.arpc >= arps) { lead.arpc = 0; lead.arpi = (lead.arpi + 1) % LARPN[arpm]; }
            f *= semis((float)LARP[arpm][lead.arpi]);
        }
        float xl, xr, d = det * 0.0116f;                /* DETUNE: a second oscillator, ~20 cents apart */
        osc_run(&lead.o, m, dual ? f * (1.0f - d) : f, tim, width, &xl, &xr);
        if (dual) {
            float yl, yr;
            osc_run(&lead.o2, m, f * (1.0f + d), tim, width, &yl, &yr);
            xl = 0.5f * (xl + yl); xr = 0.5f * (xr + yr);
        }
        /* envelope: attack to .5, then towards .5 * sustain; release */
        if (lead.gate) {
            if (lead.stage == 0) {
                lead.amp += att > 0.001f ? 0.5f / (att * FS) : 0.5f;
                if (lead.amp >= 0.5f) { lead.amp = 0.5f; lead.stage = 1; }
            } else lead.amp = 0.5f * sus + (lead.amp - 0.5f * sus) * lead.kdec;
        } else lead.amp *= lead.krel;
        if (tone) { xl = svf_run(&lead.lp, xl, 0); xr = svf_run(&lead.lp2, xr, 0); }
        float g = lead.amp * vm;
        l[i] += xl * g * gl; r[i] += xr * g * gr;
    }
    if (!lead.gate && lead.amp < 0.0003f) lead.on = 0;
}

/* ------------------------------------------------------------------ MEME voices: one per layer */
#define NMV 10
typedef struct {
    const layer_t *L;
    int on, seg, si, n_notes, rep;
    int32_t wait, t, tlen, period, tseg, notelen, notestart;
    float f, kf, pitch, ts, repamp;
    float vibd, kvib, vph, amph;
    float noise, kn, nsh, ncnt, nrate, knr, crk, kcrk;
    float jit, jcnt;
    float cut, kcut, flph; int fseg; int32_t fnext;
    float ph[4];
    svf_t flt;
    int cr;
} mv_t;
static mv_t mv[NMV];
static float meme_pmul = 1.0f;                          /* the meme LFO: pitch of every meme voice, block rate */
static int meme_ltg; static float meme_lv;              /* its target and value this block */
static svf_t meme_lpl, meme_lpr;
static int mv_rr;
static float meme_ramp_k(float f0, float f1, float sec)
{
    if (f0 <= 0.0f || f1 <= 0.0f || sec <= 0.0f) return 1.0f;
    return fexp2(flog2(f1 / f0) / (sec * FS));
}
static inline float layer_q(const layer_t *L) { return L->ftype == 2 ? (L->q > 0.0f ? L->q : 1.0f) : (L->q > 0.0f ? qdb(L->q) : 0.707f); }
static void mv_note(mv_t *v)                             /* the next note of a sequence, or the next pitch segment */
{
    const layer_t *L = v->L;
    float ts = v->ts;
    if (L->seq) {
        int count = L->count ? L->count : L->nseq;
        if (v->si >= count) { v->tseg = 0x7fffffff; return; }
        int k = L->seqrand ? (int)(rnd32() % L->nseq) : v->si % L->nseq;
        v->f = L->seq[k] * v->pitch; v->kf = 1.0f;
        float len = (v->si == count - 1 && L->lastl > 0.0f) ? L->lastl : L->seql;
        v->notestart = v->t;
        v->notelen = (int32_t)(len * ts * FS);
        if (v->notelen < 1) v->notelen = 1;
        v->tseg = v->t + v->notelen;
        v->si++;
        return;
    }
    if (v->seg < 3 && L->t[v->seg] > 0.0f && L->f[v->seg + 1] > 0.0f) {
        v->kf = meme_ramp_k(L->f[v->seg], L->f[v->seg + 1], L->t[v->seg] * ts);
        v->tseg = v->t + (int32_t)(L->t[v->seg] * ts * FS);
        v->seg++;
    } else { v->kf = 1.0f; v->tseg = 0x7fffffff; }
}
static void mv_fseg(mv_t *v)                             /* the filter's next stretch */
{
    const layer_t *L = v->L;
    float m = L->fcm > 0.0f ? L->fcm : 0.5f;
    int32_t a = (int32_t)((float)v->tlen * m);
    if (v->fseg == 0 && L->fc1 > 0.0f) {
        v->kcut = meme_ramp_k(L->fc0, L->fc1, (float)(a > 1 ? a : 1) * INV_FS); v->fnext = a; v->fseg = 1;
    } else if (v->fseg == 1 && L->fc2 > 0.0f) {
        int32_t b = v->tlen - a;
        v->kcut = meme_ramp_k(L->fc1, L->fc2, (float)(b > 1 ? b : 1) * INV_FS); v->fnext = 0x7fffffff; v->fseg = 2;
    } else { v->kcut = 1.0f; v->fnext = 0x7fffffff; v->fseg = 3; }
}
static void mv_start(mv_t *v)                            /* (re)start a layer: at the trigger and at each repeat */
{
    const layer_t *L = v->L;
    float ts = v->ts;
    v->t = 0; v->seg = 0; v->si = 0; v->cr = 0;
    if (L->seq) {
        int count = L->count ? L->count : L->nseq;
        float last = L->lastl > 0.0f ? L->lastl : L->seql;
        v->tlen = (int32_t)(((float)(count - 1) * L->seql + last) * ts * FS);
    } else v->tlen = (int32_t)((L->att + L->hold + L->rel) * ts * FS);
    if (v->tlen < 1) v->tlen = 1;
    if (v->period < v->tlen) v->period = v->tlen;
    v->f = L->f[0] * v->pitch;
    v->vibd = L->vibd; v->kvib = L->vibdec > 0.0f ? decay_k(L->vibdec * ts, -4.0f) : 1.0f;
    v->noise = L->noise; v->kn = L->noisedec > 0.0f ? decay_k(L->noisedec * ts, LN_1000) : 1.0f;
    v->nrate = L->nr0; v->knr = (L->nr0 > 0.0f && L->nr1 > 0.0f) ? meme_ramp_k(L->nr0, L->nr1, (float)v->tlen * INV_FS) : 1.0f;
    v->ncnt = 0.0f; v->nsh = 0.0f; v->crk = 0.0f; v->kcrk = decay_k(0.004f, -1.0f);
    v->jit = 1.0f; v->jcnt = 0.0f;
    v->cut = L->fc0; v->fseg = 0; v->flph = 0.0f;
    v->flt.ic1 = v->flt.ic2 = 0.0f;
    if (L->ftype) { svf_set(&v->flt, v->cut, layer_q(L)); mv_fseg(v); }
    for (int k = 0; k < 4; k++) v->ph[k] = 0.0f;
    mv_note(v);
}
static void meme_trig(int idx, int step)
{
    if (idx < 1 || idx > NMEME) return;
    const meme_t *m = &MEMES[idx - 1];
    float pitch = semis(V[5][M_PITCH] + (V[5][M_RND] > 0.0f ? (float)(int)(frnd() * V[5][M_RND]) : 0.0f));
    int stutter = PI_(5, M_MODE) == 1 && step != -1;
    int32_t sl = (int32_t)(sps * 0.5f);
    for (int l = 0; l < m->nl && l < 4; l++) {
        mv_t *v = &mv[mv_rr++ % NMV];
        const layer_t *L = &m->L[l];
        v->L = L; v->on = 1; v->pitch = pitch * semis((float)live_shift);
        v->ts = (meme_ltg == 5 && V[5][M_LD] != 0.0f) ? clampf(V[5][M_LEN] + meme_lv * 0.5f, 0.05f, 1.0f) : V[5][M_LEN];
        v->rep = L->reps > 1 ? L->reps : 1; v->repamp = 1.0f;
        v->period = (int32_t)(L->gap * v->ts * FS);
        v->wait = (int32_t)(L->delay * v->ts * FS);
        mv_start(v);
        if (stutter) {                                   /* STUTTER: short hits, half a step apart */
            if (v->wait >= sl) { v->on = 0; continue; }
            if (v->tlen > sl * 9 / 10) v->tlen = sl * 9 / 10;
            v->rep = 1;
        }
    }
    if (stutter && step >= 0) for (int k = 1; k < 4; k++) ev_add(sl * k, E_MEME, (uint8_t)idx, 0);
}
static void meme_run(float *l, float *r, int a, int b)
{
    for (int n = 0; n < NMV; n++) {
        mv_t *v = &mv[n];
        if (!v->on) continue;
        const layer_t *L = v->L;
        float ts = v->ts;
        int nosc = L->nosc > 1 ? L->nosc : 1;
        int32_t ta = (int32_t)(L->att * ts * FS), th = (int32_t)((L->att + L->hold) * ts * FS);
        float relN = L->rel * ts * FS + 1.0f;
        float k = L->drive, nk = k > 0.0f ? 1.0f / ftanh(k) : 1.0f;
        float gate = L->gate > 0.0f ? L->gate : 0.8f;
        int fmode = L->ftype == 1 ? 0 : L->ftype == 2 ? 1 : 2;
        float oscg = nosc > 1 ? 0.5f : 1.0f;
        for (int i = a; i < b; i++) {
            if (v->wait > 0) { v->wait--; continue; }
            if (v->t >= v->tlen) {                       /* done, or silent until the next repeat */
                if (v->rep > 1 && v->t >= v->period) {
                    v->rep--; v->repamp *= L->repdec > 0.0f ? L->repdec : 1.0f; mv_start(v);
                } else if (v->rep <= 1) { v->on = 0; break; }
                else { v->t++; continue; }
            }
            if (v->t >= v->tseg) mv_note(v);
            if (L->ftype && (v->cr++ & 15) == 0) {
                if (v->t >= v->fnext) mv_fseg(v);
                float fc = v->cut;
                if (L->fld > 0.0f) { fc *= fexp2(fsin1(v->flph) * L->fld); v->flph += L->flf * 16.0f * INV_FS; }
                svf_set(&v->flt, fc, layer_q(L));
                v->cut *= fexp2(flog2(v->kcut) * 16.0f);
            }
            v->f *= v->kf;
            float f = v->f * meme_pmul;
            if (v->vibd > 0.0f) { v->vph += L->vibf * INV_FS; f *= 1.0f + fsin1(v->vph) * v->vibd; v->vibd *= v->kvib; }
            if (L->jitd > 0.0f) {
                v->jcnt += L->jitf * INV_FS;
                if (v->jcnt >= 1.0f) { v->jcnt -= 1.0f; v->jit = 1.0f + frnd() * L->jitd; }
                f *= v->jit;
            }
            float x = 0.0f;
            if (L->wave != 9) {
                v->ph[0] = wrap1(v->ph[0] + f * INV_FS);
                x = wave(L->wave, v->ph[0]);
                for (int o = 1; o < nosc; o++) {
                    v->ph[o] = wrap1(v->ph[o] + f * L->ratio[o - 1] * INV_FS);
                    x += wave(L->wave, v->ph[o]);
                }
                x *= oscg;
            }
            if (v->noise > 0.0005f) {
                float nz;
                if (v->nrate > 0.0f) {
                    v->ncnt += v->nrate;
                    if (v->ncnt >= 1.0f) { v->ncnt -= (float)(int)v->ncnt; v->nsh = frnd(); }
                    nz = v->nsh; v->nrate *= v->knr;
                } else nz = frnd();
                if (L->crackle > 0.0f) {
                    if (frnd01() < L->crackle * INV_FS) v->crk = 0.4f + frnd01() * 0.8f;
                    nz *= v->crk; v->crk *= v->kcrk;
                }
                x += nz * v->noise;
                v->noise *= v->kn;
            }
            if (L->ftype) x = svf_run(&v->flt, x, fmode);
            float env;
            if (L->seq) {                                /* each note gated, all of it under att / hold / rel */
                int32_t into = v->t - v->notestart, on = (int32_t)((float)v->notelen * gate);
                int32_t fa = (int32_t)(0.002f * FS) + 1;
                env = into < fa ? (float)into / (float)fa : 1.0f;
                if (gate < 1.0f && into > on) { float d = 1.0f - (float)(into - on) / (0.005f * FS + 1.0f); env = d > 0.0f ? d : 0.0f; }
                if (L->att > 0.0f && v->t < ta) env *= (float)v->t / (float)(ta + 1);
                if (L->rel > 0.0f && v->t > th) env *= fexp((float)(v->t - th) / relN * LN_1000);
            } else if (v->t < ta) env = (float)v->t / (float)(ta + 1);
            else if (v->t < th) env = 1.0f;
            else env = fexp((float)(v->t - th) / relN * LN_1000);
            if (L->amd > 0.0f) {
                v->amph += L->amf * INV_FS;
                float m = L->amsq ? (fsin1(v->amph) > 0.0f ? 1.0f : 0.0f) : 0.5f + 0.5f * fsin1(v->amph);
                env *= 1.0f - L->amd * (1.0f - m);
            }
            x *= env * L->amp * v->repamp;
            if (k > 0.0f) x = ftanh(k * x) * nk;
            l[i] += x; r[i] += x;
            v->t++;
        }
    }
}

/* ------------------------------------------------------------------ sequencer + render */
static int32_t step_in;                                  /* samples until the next step */
static float step_acc;
static int seq_step, first_bar;                                     /* the step that plays next */
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
                if (LIVE[p].s[t][i]) return p;
    }
    return pat_play;
}
static void play_step(int s)
{
    if (s == 0) { if (first_bar) first_bar = 0; else pat_play = (uint8_t)pick_pattern(); }
    const pat_t *p = &LIVE[pat_play];
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
        case R_START: playing = 1; pat_play = pat_next; first_bar = 1; seq_step = 0; step_in = 0; step_acc = 0.0f; lfo_pos = 0.0f; step_count = 0; break;
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
    {   /* the meme LFO, once a block */
        int tg; float v = lfo_val(5, M_LT, M_LR, M_LD, M_LS, lfo_pos, 333, &tg);
        meme_ltg = V[5][M_LD] != 0.0f ? tg : -1; meme_lv = v;
        meme_pmul = meme_ltg == 0 ? semis(v * 12.0f) : 1.0f;
    }
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

    /* the meme LFO on the MEME track: volume, tone, pan */
    if (meme_ltg >= 1 && meme_ltg <= 3) {
        float v = meme_lv, g = meme_ltg == 1 ? clampf(1.0f + v, 0.0f, 2.0f) : 1.0f;
        float pl = meme_ltg == 3 && v > 0.0f ? 1.0f - v : 1.0f, pr = meme_ltg == 3 && v < 0.0f ? 1.0f + v : 1.0f;
        if (meme_ltg == 2) {
            svf_set(&meme_lpl, clampf(2500.0f * fexp2(v * 2.3f), 150.0f, 16000.0f), qdb(3.0f));
            meme_lpr.g = meme_lpl.g; meme_lpr.k = meme_lpl.k; meme_lpr.a1 = meme_lpl.a1; meme_lpr.a2 = meme_lpl.a2; meme_lpr.a3 = meme_lpl.a3;
            for (uint32_t i = 0; i < n; i++) { TL[T_MEME][i] = svf_run(&meme_lpl, TL[T_MEME][i], 0); TR_[T_MEME][i] = svf_run(&meme_lpr, TR_[T_MEME][i], 0); }
        } else for (uint32_t i = 0; i < n; i++) { TL[T_MEME][i] *= g * pl; TR_[T_MEME][i] *= g * pr; }
    }
    /* mix: volume, pan, mute/solo (smooth), echo send */
    float gl[NTR], gr[NTR], snd[NTR], tg[NTR];
    for (int t = 0; t < NTR; t++) {
        float pan = V[CTX_MIX][8 + t];
        gl[t] = pan > 0.0f ? 1.0f - pan : 1.0f; gr[t] = pan < 0.0f ? 1.0f + pan : 1.0f;
        snd[t] = *pval(CTX_MIX, 16 + t);
        if (t == T_MEME && meme_ltg == 4) snd[t] = clampf(snd[t] + meme_lv, 0.0f, 1.0f);
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
    if (H->bank_load(&store, sizeof store) != 0 || store.magic != STORE_MAGIC || store.version != 2) return -1;
    for (int p = 0; p < NPAT; p++) { PAT[p] = store.pat[p]; LIVE[p] = PAT[p]; pending[p] = 0; }
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
    store.magic = STORE_MAGIC; store.version = 2;
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
static void cat_(char *d, const char *s) { while (*d) d++; while (*s) *d++ = *s++; *d = 0; }
static void fmt_num_(char *d, int v) { char b[12]; int n = 0; while (*d) d++; if (v < 0) { *d++ = '-'; v = -v; } do { b[n++] = (char)('0' + v % 10); v /= 10; } while (v && n < 11); while (n) *d++ = b[--n]; *d = 0; }
static void flash_msg(const char *s) { msg = s; msg_until = H->ms() + 1200u; if (!msg_until) msg_until = 1; dirty |= D_FOOT; }
/* after an edit of PAT[p]: heard at once when p is the active pattern (not in a chain), else kept until p is activated */
static void edited(int p)
{
    if (!chain_on && p == pat_next) { LIVE[p] = PAT[p]; pending[p] = 0; }
    else pending[p] = 1;
    dirty |= D_HEAD;
}
/* a pattern key: the first press shows and edits that pattern, the next press activates it */
static void pattern_key(int p)
{
    static char m[24];
    if (p != pat_edit) {
        pat_edit = (uint8_t)p;
        m[0] = 0; cat_(m, "P"); fmt_num_(m, p + 1); cat_(m, ": 2x = AKTIV");
    } else {
        LIVE[p] = PAT[p]; pending[p] = 0;
        if (!chain_on) { pat_next = (uint8_t)p; if (!playing) pat_play = (uint8_t)p; }
        m[0] = 0; cat_(m, "P"); fmt_num_(m, p + 1); cat_(m, playing && !chain_on && pat_play != p ? " AB TAKTENDE" : " AKTIV");
    }
    flash_msg(m);
    dirty |= D_ALL;
}

static void leds_update(void)
{
    static uint8_t shown[27 + 14];
    static int init;
    uint8_t want[27 + 14];
    const uint8_t *st_ = PAT[pat_edit].s[track];
    int st = playing && pat_play == pat_edit ? cur_step : -1, blink = (int)((H->ms() / 250u) & 1u);
    for (int k = 0; k < 27; k++) want[k] = 0;
    if (step_mode) {
        for (int i = 0; i < NSTEP; i++) { uint8_t on = st_[i] ? 1 : 0; if (i == st) on = !on; want[STEP_KEY[i]] = on; }
        for (int k = 0; k < 27; k++) if (KEY_BLACK[k] >= 0) {
            int b = KEY_BLACK[k];
            if (b < 6) want[k] = (uint8_t)(mute[b] ? 0 : solo[b] ? 1 : 2);
            else if (b < 10) {                           /* lit: plays; blinking: being edited, not active */
                int p = b - 6, active = chain_on ? (playing && pat_play == p) : pat_next == p;
                want[k] = (uint8_t)(active ? 1 : p == pat_edit ? (blink ? 1 : 0) : 2);
            } else want[k] = (uint8_t)(chain_on ? 1 : 0);
        }
    }
    for (int i = 0; i < 14; i++) want[27 + i] = 0;
    want[27 + HB_BTN_PLAY] = playing ? 1 : 0;
    want[27 + HB_BTN_REC] = rec ? 1 : 0;
    want[27 + HB_BTN_SEQ] = (uint8_t)step_mode;
    for (int k = 0; k < 27 + 14; k++)
        if (!init || want[k] != shown[k]) {
            if (k < 27) H->led(HB_LED_KEY(k), want[k]);
            else if (k - 27 == HB_BTN_PLAY || k - 27 == HB_BTN_REC || k - 27 == HB_BTN_SEQ) H->led((uint32_t)(k - 27), want[k]);
            shown[k] = want[k];
        }
    init = 1;
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
        default: a[i] = (uint8_t)(r < 0.2f ? ((meme_sel - 1) / 16) * 16 + 1 + (int)(r2 * 15.99f) : 0); break;
        }
    }
    edited(pat_edit);
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
    edited(pat_edit);
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
    default:
        if (KEY_STEP[k] >= 0) { meme_sel = ((meme_sel - 1) / 16) * 16 + KEY_STEP[k] + 1; post(R_MEME, (uint8_t)meme_sel, 0); }
        else if (KEY_BLACK[k] < 6) { meme_sel = KEY_BLACK[k] * 16 + (meme_sel - 1) % 16 + 1; post(R_MEME, (uint8_t)meme_sel, 0); flash_msg(BANKN[KEY_BLACK[k]]); }
        else { static const int SH[5] = {-12, -5, 0, 7, 12}; live_shift = SH[KEY_BLACK[k] - 6]; }
        dirty |= D_FOOT;
    }
    if (rec) {
        uint8_t *s = PAT[pat_edit].s[track];
        int v = track == T_LEAD ? note : track == T_MEME ? meme_sel : track == T_HAT ? (black ? 2 : 1) : track == T_KICK ? (black ? 2 : 1) : 1;
        if (track == T_MEME && KEY_STEP[k] < 0) return;
        if (playing) { int at = cur_step < 0 ? 0 : cur_step; s[at] = (uint8_t)v; }
        else { s[lead_cur] = (uint8_t)v; lead_cur = (lead_cur + 1) % NSTEP; }
        edited(pat_edit);
        dirty |= D_GRID;
    }
}
static void tap_tempo(void)
{
    static uint32_t last, iv[4]; static int n;
    uint32_t now = H->ms(), d = now - last;
    last = now;
    if (d < 250u || d > 1500u) { n = 0; flash_msg("TAP"); return; }
    iv[n % 4] = d; n++;
    uint32_t sum = 0; int c = n < 4 ? n : 4;
    for (int i = 0; i < c; i++) sum += iv[i];
    float bpm = clampf(60000.0f * (float)c / (float)sum, 60.0f, 200.0f);
    V[CTX_GLO][G_BPM] = (float)(int)(bpm + 0.5f);
    static char m[16]; m[0] = 0; cat_(m, "TAP "); fmt_num_(m, (int)V[CTX_GLO][G_BPM]); cat_(m, " BPM");
    flash_msg(m);
    dirty |= D_HEAD | D_PAR;
}
static void shift_track(int dir)
{
    uint8_t *a = PAT[pat_edit].s[track], t;
    if (dir > 0) { t = a[NSTEP - 1]; for (int i = NSTEP - 1; i > 0; i--) a[i] = a[i - 1]; a[0] = t; }
    else { t = a[0]; for (int i = 0; i < NSTEP - 1; i++) a[i] = a[i + 1]; a[NSTEP - 1] = t; }
    edited(pat_edit);
    flash_msg(dir > 0 ? "PATTERN >>" : "<< PATTERN");
    dirty |= D_GRID;
}
static void ui(uint32_t ev, int32_t arg)
{
    uint32_t kind = HB_EV_KIND(ev), id = HB_EV_ID(ev);
    static uint32_t held, sel_down, home_down;           /* buttons held */
    static int sel_used;
    if ((kind == HB_EV_BTN_DOWN && id != HB_BTN_SEL) || kind == HB_EV_KEY_DOWN || kind == HB_EV_ENC)
        if (held & (1u << HB_BTN_SEL)) sel_used = 1;
    if (kind == HB_EV_BTN_DOWN) {
        held |= 1u << id;
        switch (id) {
        case HB_BTN_SEL: sel_down = H->ms(); sel_used = 0; break;
        case HB_BTN_HOME: home_down = H->ms(); break;
        case HB_BTN_PLAY: post(playing ? R_STOP : R_START, 0, 0); dirty |= D_HEAD | D_GRID; break;
        case HB_BTN_REC: rec = !rec; dirty |= D_FOOT; break;
        case HB_BTN_SEQ: step_mode = !step_mode; dirty |= D_FOOT; break;
        case HB_BTN_EDIT: {                             /* copy the pattern into the next one, go there */
            int to = (pat_edit + 1) % NPAT;
            PAT[to] = PAT[pat_edit];
            pat_edit = (uint8_t)to;
            edited(to);
            static char m[24] = "KOPIE->P1 1x=AKTIV"; m[8] = (char)('1' + to); flash_msg(m);
            dirty |= D_ALL; break;
        }
        case HB_BTN_ENV: ctx = track; page = 0; dirty |= D_ALL; break;
        case HB_BTN_FX: ctx = CTX_MIX; page = 0; dirty |= D_ALL; break;
        case HB_BTN_GLO: ctx = CTX_GLO; page = 0; dirty |= D_ALL; break;
        case HB_BTN_LFO:                                 /* the track's LFO page, where it has one */
            ctx = track;
            page = track == T_BASS ? 4 : track == T_ARP ? 2 : track == T_LEAD ? 3 : track == T_MEME ? 2 : 0;
            dirty |= D_ALL; break;
        case HB_BTN_ARP:
            if (held & (1u << HB_BTN_SEL)) { for (int i = 0; i < NSTEP; i++) PAT[pat_edit].s[track][i] = 0; edited(pat_edit); flash_msg("SPUR GELOESCHT"); }
            else { randomise(); flash_msg("DICE!"); }
            dirty |= D_GRID; break;
        case HB_BTN_OCT_DOWN: if (step_mode) { shift_track(-1); break; } if (octave > -2) octave--; flash_msg(octave == -2 ? "OKT -2" : octave == -1 ? "OKT -1" : octave == 0 ? "OKT 0" : "OKT +1"); break;
        case HB_BTN_OCT_UP: if (step_mode) { shift_track(1); break; } if (octave < 2) octave++; flash_msg(octave == 2 ? "OKT +2" : octave == 1 ? "OKT +1" : octave == 0 ? "OKT 0" : "OKT -1"); break;
        case HB_BTN_SAVE:
            if (playing) { flash_msg("ERST STOP, DANN SAVE"); break; }
            flash_msg(store_save() == 0 ? "GESPEICHERT" : "SPEICHERN FEHLGESCHLAGEN"); break;
        default: break;
        }
    } else if (kind == HB_EV_BTN_UP) {
        held &= ~(1u << id);
        if (id == HB_BTN_SEL && !sel_used && H->ms() - sel_down < 400u) tap_tempo();
        if (id == HB_BTN_HOME && H->ms() - home_down < 800u) {   /* back home: the track's first page, STEP mode */
            ctx = track; page = 0; step_mode = 1; flash_msg("HOME"); dirty |= D_ALL;
        }
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
                } else if (b < 10) pattern_key(b - 6);
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
    char s[40] = "MEMETEK FM1 P", b[12];
    fmt_num(b, pat_edit + 1, 0); cat(s, b);
    if (pending[pat_edit]) cat(s, "*");                  /* edits not yet activated */
    H->cv_begin(240, 26, MC_BG);
    H->cv_text(4, 4, 0, s, MC_TXT);
    s[0] = 0;
    cat(s, playing ? ">P" : "||P");
    fmt_num(b, (playing ? pat_play : pat_next) + 1, 0); cat(s, b);
    if (chain_on) cat(s, "C");
    cat(s, " ");
    fmt_num(b, (int)(V[CTX_GLO][G_BPM] + 0.5f), 0); cat(s, b);
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
            else if (track == T_ARP) { static const char *const AL[4] = {"", "I", "IV", "V"}; const char *t = AL[a[i] & 3]; lb[0] = t[0]; lb[1] = t[1]; }
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
        if (t == track) H->cv_rect(2 + t * 14, 4, 13, 20, HB_RGB(64, 64, 76));
        cv_center(2 + t * 14, 13, 6, 0, l, c);
    }
    if (msg && (int32_t)(H->ms() - msg_until) < 0) cv_right(236, 6, 0, msg, HB_RGB(255, 210, 62));
    else {
        char b[8];
        s[0] = 0;
        cat(s, step_mode ? "STEP " : "PLAY ");
        b[0] = (char)('A' + (meme_sel - 1) / 16); b[1] = ':'; b[2] = 0; cat(s, b);
        cat(s, MEMES[meme_sel - 1].name);
        if (live_shift) { fmt_num(b, live_shift, 1); cat(s, " "); cat(s, b); }
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
    static int was_pat = -1;
    if (was_pat != pat_play) { was_pat = pat_play; dirty |= D_HEAD | D_GRID; }
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
    for (int p = 0; p < NPAT; p++) { LIVE[p] = PAT[p]; pending[p] = 0; }
    pat_edit = pat_next = pat_play = 0;
    for (int t = 0; t < NTR; t++) trk_g[t] = 0.0f;
    dirty = 0xFFFFFFFFu;
    H->log("memetek fm1: ready");
    return 0;
}
static void release(void)
{
    playing = 0; kick.on = 0; bass.on = 0; arp.on = 0; lead.on = 0;
    for (int v = 0; v < NHAT; v++) hats[v].on = 0;
    for (int v = 0; v < NMV; v++) mv[v].on = 0;
}
static const hb_param_t PARAMS[] = {{"BPM", 60, 200, 160}};
HB_DESC = {HB_ABI, "MEMETEK FM1", init, release, note, cc, render, ui, draw, PARAMS, 1, HB_F_SCREEN | HB_F_PANEL, 0, 0};
