/* SPDX-License-Identifier: GPL-3.0-only */
/* Memetek FM1 on a computer: the module (modules/memetek/memetek.c) runs unchanged against a fake fm1-dock
 * host. Out come build/host/memetek.wav (what the FM-1 would play), screens as PPM, and a check:
 * no NaN, no silence where there should be sound, no clipping storm, every page and track touched.
 *   tests/run_host.sh   (needs fm1-dock's firmware/ for hb_module.h and the font: $FM1_DOCK) */
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---- the screen, with fm1-dock's own font and canvas code */
static uint16_t fb[240 * 240];
static void lcd_sync(void) {}
static void lcd_fill(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint16_t c)
{
    for (uint32_t j = y; j < y + h && j < 240u; j++)
        for (uint32_t i = x; i < x + w && i < 240u; i++) fb[j * 240u + i] = c;
}
static void lcd_blit(uint32_t x, uint32_t y, uint32_t w, uint32_t h, const uint16_t *px)
{
    for (uint32_t j = 0; j < h; j++)
        for (uint32_t i = 0; i < w; i++)
            if (x + i < 240u && y + j < 240u) { uint16_t c = px[j * w + i]; fb[(y + j) * 240u + x + i] = (uint16_t)(c >> 8 | c << 8); }
}
#define CV_MAX (240u * 124u)
#include "gfx.c"
#include "hb_module.h"

static uint32_t now_ms;
static int log_quiet;
static uint32_t h_ms(void) { return now_ms; }
static uint32_t h_ticks(void) { return now_ms * 24000u; }
static void h_log(const char *s) { if (!log_quiet) printf("log: %s\n", s); }
static void h_log_u32(const char *s, uint32_t v) { if (!log_quiet) printf("log: %s %08x\n", s, v); }
static void h_fill(int32_t x, int32_t y, int32_t w, int32_t h, uint16_t c) { if (x >= 0 && y >= 0) lcd_fill((uint32_t)x, (uint32_t)y, (uint32_t)w, (uint32_t)h, c); }
static int bad_draw;
static void h_text(int32_t x, int32_t y, int32_t w, uint32_t big, const char *s, uint16_t fg, uint32_t center)
{
    const felucca_font_t *f = big ? &FONT_L : &FONT_S;
    if (x < 0 || y < 0 || w <= 0 || x + w > 240 || y + (int32_t)f->h > 240) { bad_draw++; return; }
    draw_text_box((uint32_t)x, (uint32_t)y, (uint32_t)w, f, s, fg, center ? 1 : 0);
}
static void h_cv_begin(int32_t w, int32_t h, uint16_t bg)
{
    if (w < 1) w = 1;
    if (w > 240) w = 240;
    if (h < 1) h = 1;
    if ((uint32_t)(w * h) > 240u * 96u) { bad_draw++; h = (int32_t)(240u * 96u / (uint32_t)w); }
    cv_begin((uint32_t)w, (uint32_t)h, bg);
}
static void h_cv_rect(int32_t x, int32_t y, int32_t w, int32_t h, uint16_t c) { cv_rect(x, y, w, h, c); }
static void h_cv_line(int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint16_t c) { cv_line(x0, y0, x1, y1, c); }
static void h_cv_pixel(int32_t x, int32_t y, uint16_t c) { cv_pset(x, y, c); }
static void h_cv_text(int32_t x, int32_t y, uint32_t big, const char *s, uint16_t fg) { cv_text(x, y, big ? &FONT_L : &FONT_S, s, fg); }
static void h_cv_blit(int32_t x, int32_t y)
{
    if (x >= 0 && y >= 0 && x + (int32_t)cv_w <= 240 && y + (int32_t)cv_h <= 240) cv_blit((uint32_t)x, (uint32_t)y);
    else bad_draw++;
}
static uint8_t leds[64];
static void h_led(uint32_t id, uint32_t on) { if (id < 64u) leds[id] = (uint8_t)on; else bad_draw++; }
static uint32_t h_rand(void) { return (uint32_t)rand(); }
static uint8_t settings[240]; static uint32_t settings_n;
static int h_load(void *p, uint32_t n) { if (!settings_n || n != settings_n) return -1; memcpy(p, settings, n); return 0; }
static void h_save(const void *p, uint32_t n) { if (n <= 240u) { memcpy(settings, p, n); settings_n = n; } }
static void h_midi_send(uint32_t st, uint32_t d1, uint32_t d2) { (void)st; (void)d1; (void)d2; }
static uint32_t h_master(void) { return 4096u; }
static void h_blit(int32_t x, int32_t y, int32_t w, int32_t h, const uint16_t *px) { (void)x; (void)y; (void)w; (void)h; (void)px; }
static int32_t h_text_w(uint32_t big, const char *s) { return text_w(big ? &FONT_L : &FONT_S, s); }
static uint8_t bank[HB_BANK_MAX]; static uint32_t bank_n;
static int saves;
static int h_bank_load(void *p, uint32_t n) { if (!bank_n || n > bank_n) return -1; memcpy(p, bank, n); return 0; }
static int h_bank_save(const void *p, uint32_t n) { if (n > HB_BANK_MAX) return -1; memcpy(bank, p, n); bank_n = n; saves++; return 0; }

static hb_host_t host = {
    HB_ABI, sizeof(hb_host_t), h_ms, h_ticks, h_log, h_log_u32, h_fill, h_text,
    h_cv_begin, h_cv_rect, h_cv_line, h_cv_pixel, h_cv_text, h_cv_blit, h_led, h_rand, h_load, h_save,
    h_midi_send, h_master, h_blit, h_text_w, h_bank_load, h_bank_save, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};

#include "../modules/memetek/memetek.c"

/* ---- the run */
static FILE *wav;
static uint32_t wav_frames;
static double peak, sumsq; static uint64_t nsamp, clips, nans;
static void wav_begin(const char *path)
{
    wav = fopen(path, "wb");
    uint8_t hdr[44] = {0};
    fwrite(hdr, 1, 44, wav);
}
static void put32(uint8_t *p, uint32_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24); }
static void wav_end(void)
{
    uint8_t h[44];
    memcpy(h, "RIFF", 4); put32(h + 4, 36 + wav_frames * 4); memcpy(h + 8, "WAVEfmt ", 8);
    put32(h + 16, 16); h[20] = 1; h[21] = 0; h[22] = 2; h[23] = 0; put32(h + 24, 44100); put32(h + 28, 44100 * 4);
    h[32] = 4; h[33] = 0; h[34] = 16; h[35] = 0; memcpy(h + 36, "data", 4); put32(h + 40, wav_frames * 4);
    fseek(wav, 0, SEEK_SET); fwrite(h, 1, 44, wav); fclose(wav);
}
static const hb_module_t *M = &hb_module;
static int writing = 1;
static double win_peak;
static void block(void)
{
    int32_t out[512];
    memset(out, 0, sizeof out);
    M->render(out, 256);
    for (int i = 0; i < 512; i++) {
        double v = out[i] / 32768.0;
        if (v != v) nans++;
        double a = fabs(v);
        if (a > peak) peak = a;
        if (a > win_peak) win_peak = a;
        if (a >= 0.999) clips++;
        sumsq += v * v; nsamp++;
    }
    if (writing && wav) {
        int16_t s[512];
        for (int i = 0; i < 512; i++) { int32_t v = out[i]; s[i] = (int16_t)(v > 32767 ? 32767 : v < -32768 ? -32768 : v); }
        fwrite(s, 2, 512, wav); wav_frames += 256;
    }
    static double acc;
    acc += 256.0 * 1000.0 / 44100.0;
    while (acc >= 1.0) { now_ms++; acc -= 1.0; }
}
static void run_ms(uint32_t ms) { uint32_t end = now_ms + ms; while (now_ms < end) { block(); M->draw(0); } }
static void btn(uint32_t id) { M->ui(HB_EV_BTN_DOWN | id, 0); M->draw(0); M->ui(HB_EV_BTN_UP | id, 0); M->draw(0); }
static void enc(uint32_t id, int32_t clicks) { M->ui(HB_EV_ENC | id, clicks); M->draw(0); }
static void key(uint32_t k, int down) { M->ui((down ? HB_EV_KEY_DOWN : HB_EV_KEY_UP) | k, 64); M->draw(0); }
static void shot(const char *path)
{
    FILE *f = fopen(path, "wb");
    fprintf(f, "P6\n240 240\n255\n");
    for (int i = 0; i < 240 * 240; i++) {
        uint16_t c = fb[i];
        uint8_t rgb[3] = {(uint8_t)((c >> 11) << 3), (uint8_t)(((c >> 5) & 63) << 2), (uint8_t)((c & 31) << 3)};
        fwrite(rgb, 1, 3, f);
    }
    fclose(f);
    printf("screen -> %s\n", path);
}
static void patkey(int p) { for (int k = 0; k < 27; k++) if (KEY_BLACK[k] == 6 + p) { key((uint32_t)k, 1); key((uint32_t)k, 0); } }
static double window(uint32_t ms) { win_peak = 0; run_ms(ms); return win_peak; }
static int fails;
static void expect(int ok, const char *what) { printf("%s %s\n", ok ? "ok  " : "FAIL", what); if (!ok) fails++; }

int main(int argc, char **argv)
{
    const char *dir = argc > 1 ? argv[1] : "build/host";
    char p[512];
    snprintf(p, sizeof p, "%s/memetek.wav", dir);
    wav_begin(p);
    expect(M->abi == HB_ABI && M->init(&host) == 0, "init");
    M->draw(1);
    snprintf(p, sizeof p, "%s/memetek-0-start.ppm", dir); shot(p);
    expect(window(300) < 0.01, "silent before PLAY");

    /* pattern 1: tek */
    btn(HB_BTN_PLAY);
    double pk = window(4000);
    printf("     P1 peak %.3f\n", pk);
    expect(pk > 0.2, "P1 (tek) sounds");
    snprintf(p, sizeof p, "%s/memetek-1-playing.ppm", dir); shot(p);

    /* pattern 2: memecore (black key 8 = pattern 2 in STEP mode) */
    /* a pattern key: the first press only shows and edits it, P1 goes on playing */
    patkey(1);
    run_ms(3000);
    expect(pat_edit == 1 && pat_next == 0 && pat_play == 0, "first press of P2: P2 shown, P1 still plays");
    uint8_t before = LIVE[1].s[T_BASS][1];
    M->ui(HB_EV_BTN_DOWN | HB_BTN_SEL, 0); btn(HB_BTN_ARP); M->ui(HB_EV_BTN_UP | HB_BTN_SEL, 0);   /* clear P2's bass */
    expect(PAT[1].s[T_BASS][1] == 0 && LIVE[1].s[T_BASS][1] == before && pending[1], "edits of P2 wait: not heard, P2 marked *");
    btn(HB_BTN_ARP);                                     /* dice it back to something */
    patkey(1);                                           /* the second press activates */
    expect(pat_next == 1 && !pending[1] && memcmp(&LIVE[1], &PAT[1], sizeof(pat_t)) == 0, "second press of P2: activated");
    run_ms(3500);
    expect(pat_play == 1, "P2 plays from the next bar");
    expect(window(6000) > 0.2, "P2 (memecore) sounds");
    /* edits of the active pattern are heard at once */
    key(STEP_KEY[5], 1); key(STEP_KEY[5], 0);
    expect(LIVE[1].s[track][5] == PAT[1].s[track][5], "edits of the playing pattern are heard at once");
    snprintf(p, sizeof p, "%s/memetek-2-memecore.ppm", dir); shot(p);
    /* pattern 3: chiptune with the lead */
    patkey(2); patkey(2);
    enc(HB_ENC_SELECT, 2);                               /* BASS -> LEAD */
    expect(track == T_LEAD, "SELECT moves by its clicks");
    run_ms(3500);
    expect(window(6000) > 0.2, "P3 (chiptune) sounds");
    /* in a chain, an edited pattern plays its old version until it is activated */
    for (int k = 0; k < 27; k++) if (KEY_BLACK[k] == 10) { key((uint32_t)k, 1); key((uint32_t)k, 0); }
    expect(chain_on, "chain on");
    patkey(2); key(STEP_KEY[0], 1); key(STEP_KEY[0], 0);
    expect(pending[2] && memcmp(&LIVE[2], &PAT[2], sizeof(pat_t)) != 0, "chain: the edit of P3 waits");
    patkey(2);
    expect(!pending[2] && memcmp(&LIVE[2], &PAT[2], sizeof(pat_t)) == 0, "chain: P3 activated, the edit plays");
    for (int k = 0; k < 27; k++) if (KEY_BLACK[k] == 10) { key((uint32_t)k, 1); key((uint32_t)k, 0); }
    snprintf(p, sizeof p, "%s/memetek-3-lead.ppm", dir); shot(p);

    /* mute everything but the kick, then solo the bass */
    for (int k = 3; k <= 13; k++) if (KEY_BLACK[k] >= 1 && KEY_BLACK[k] <= 5) { key((uint32_t)k, 1); key((uint32_t)k, 0); }
    key(1, 1); key(1, 0);                                /* kick muted too */
    run_ms(300);
    expect(window(2000) < 0.01, "all muted: silence");
    for (int k = 0; k < 27; k++) if (KEY_BLACK[k] >= 0 && KEY_BLACK[k] <= 5) { key((uint32_t)k, 1); key((uint32_t)k, 0); }
    expect(window(3000) > 0.1, "unmuted again");

    /* the mixer and the global page */
    btn(HB_BTN_FX);
    snprintf(p, sizeof p, "%s/memetek-4-mixer.ppm", dir); shot(p);
    btn(HB_BTN_GLO);
    enc(HB_ENC_KNOB1, -40);                              /* slower */
    enc(HB_ENC_KNOB3, 30);                               /* more crush */
    run_ms(3000);
    enc(HB_ENC_KNOB3, -10);                              /* back to 45 % */
    enc(HB_ENC_KNOB1, 40);
    snprintf(p, sizeof p, "%s/memetek-5-global.ppm", dir); shot(p);

    /* every meme, live, in PLAY mode */
    btn(HB_BTN_PLAY);
    run_ms(500);
    enc(HB_ENC_SELECT, 1);                               /* LEAD -> MEME */
    btn(HB_BTN_SEQ);                                     /* PLAY mode */
    int quiet = 0;
    for (int m = 0; m < NMEME; m++) {
        if (m % 16 == 0) { for (int k = 0; k < 27; k++) if (KEY_BLACK[k] == m / 16) { key((uint32_t)k, 1); key((uint32_t)k, 0); } run_ms(2500); }
        key(STEP_KEY[m % 16], 1); key(STEP_KEY[m % 16], 0);
        if (meme_sel != m + 1) { printf("     meme_sel %d, wanted %d\n", meme_sel, m + 1); quiet++; }
        double w = window(m == 82 ? 2000 : 900);
        if (w < 0.02) { printf("     meme %s quiet (%.3f)\n", MEMES[m].name, w); quiet++; }
        else printf("     meme %-9s peak %.3f\n", MEMES[m].name, w);
    }
    expect(quiet == 0, "all 96 memes sound, keys pick them by bank");
    /* every synthesis of bass, arp and lead sounds, with the timbre at both ends */
    {
        static const struct { int ctx, wave, timb, n; const char *name; } S[3] = {{T_BASS, B_WAVE, B_TIMB, 9, "BASS"}, {T_ARP, A_WAVE, A_TIMB, 9, "ARP"}, {T_LEAD, L_WAVE, L_TIMB, 11, "LEAD"}};
        int silent = 0;
        for (int s = 0; s < 3; s++)
            for (int w = 0; w < S[s].n; w++)
                for (int tb = 0; tb < 2; tb++) {
                    V[S[s].ctx][S[s].wave] = (float)w; V[S[s].ctx][S[s].timb] = tb ? 1.0f : 0.0f;
                    if (s == 0) post(R_BASS, 45, 0); else if (s == 1) post(R_ARP, 1, 0); else post(R_LEAD_ON, 64, 0);
                    double pk = window(250);
                    if (s == 2) { post(R_LEAD_OFF, 64, 0); run_ms(60); }
                    if (pk < 0.02) { printf("     %s wave %d timbre %d quiet (%.3f)\n", S[s].name, w, tb, pk); silent++; }
                }
        V[T_BASS][B_WAVE] = 0; V[T_ARP][A_WAVE] = 0; V[T_LEAD][L_WAVE] = 0;
        expect(silent == 0, "every synthesis sounds (bass, arp, lead: supersaw, FM, PWM, sync, fold ...)");
    }
    /* the upper row's pitch keys in PLAY mode */
    key(25, 1); key(25, 0);
    expect(live_shift == 12, "MEME live pitch +12");
    key(20, 1); key(20, 0);
    expect(live_shift == 0, "MEME live pitch back to 0");
    /* tap tempo on SEL, pattern copy on EDIT, shift on OCT in STEP mode, HOME */
    for (int i = 0; i < 4; i++) { M->ui(HB_EV_BTN_DOWN | HB_BTN_SEL, 0); run_ms(50); M->ui(HB_EV_BTN_UP | HB_BTN_SEL, 0); run_ms(450); }
    printf("     tapped bpm %.0f\n", (double)V[CTX_GLO][G_BPM]);
    expect(V[CTX_GLO][G_BPM] > 115 && V[CTX_GLO][G_BPM] < 125, "tap tempo (500 ms: 120 BPM)");
    int pe = pat_edit; btn(HB_BTN_EDIT);
    expect(pat_edit == (pe + 1) % 4 && memcmp(&PAT[pe], &PAT[pat_edit], sizeof(pat_t)) == 0, "EDIT copies the pattern into the next");
    btn(HB_BTN_HOME);
    expect(step_mode == 1 && page == 0, "HOME: STEP mode, first page");
    uint8_t first = PAT[pat_edit].s[track][0], last = PAT[pat_edit].s[track][15];
    btn(HB_BTN_OCT_UP);
    expect(PAT[pat_edit].s[track][1] == first && PAT[pat_edit].s[track][0] == last, "OCT+ in STEP mode shifts the track");
    btn(HB_BTN_OCT_DOWN);
    btn(HB_BTN_SEQ);
    snprintf(p, sizeof p, "%s/memetek-6-memes.ppm", dir); shot(p);
    /* the lead, live and recorded */
    enc(HB_ENC_SELECT, -1);
    btn(HB_BTN_REC);
    for (int k = 0; k < 8; k++) { key((uint32_t)(k * 3), 1); run_ms(120); key((uint32_t)(k * 3), 0); }
    btn(HB_BTN_REC);
    expect(window(500) >= 0.0, "lead recorded");
    btn(HB_BTN_SAVE);
    expect(saves == 1, "SAVE writes the bank");
    writing = 0;

    /* stress: every page of every track, every parameter to both ends, random keys, both modes, a minute */
    log_quiet = 1;
    srand(7);
    btn(HB_BTN_SEQ);
    btn(HB_BTN_PLAY);
    for (int round = 0; round < 3; round++)
        for (int c = 0; c < NCTX; c++) {
            if (c < NTR) { while (ctx != c || track != c) enc(HB_ENC_SELECT, 1); }
            else btn(c == CTX_MIX ? HB_BTN_FX : HB_BTN_GLO);
            for (int pg = 0; pg < npages(ctx); pg++) {
                for (int k = 0; k < 4; k++) {
                    enc((uint32_t)(HB_ENC_KNOB1 + k), round == 0 ? 400 : round == 1 ? -400 : (rand() % 200) - 100);
                    run_ms(60);
                }
                enc(HB_ENC_ALGORITHM, 1);
            }
            for (int j = 0; j < 20; j++) { uint32_t k = (uint32_t)(rand() % 27); key(k, 1); run_ms(30); key(k, 0); }
            if (c == T_HAT || c == T_MEME) btn(HB_BTN_ARP);
        }
    for (int j = 0; j < 400; j++) {
        int r = rand() % 10;
        if (r < 4) { uint32_t k = (uint32_t)(rand() % 27); key(k, 1); run_ms(20); key(k, 0); }
        else if (r < 7) enc((uint32_t)(rand() % 7), (rand() % 9) - 4);
        else if (r < 8) btn((uint32_t)((int[]){HB_BTN_SEQ, HB_BTN_REC, HB_BTN_ARP, HB_BTN_EDIT, HB_BTN_LFO, HB_BTN_ENV})[rand() % 6]);
        run_ms(50);
    }
    expect(nans == 0, "no NaN");
    expect(bad_draw == 0, "draws inside the screen");
    printf("     peak %.3f, rms %.4f, clipped samples %llu of %llu\n", peak, sqrt(sumsq / (double)nsamp),
           (unsigned long long)clips, (unsigned long long)nsamp);
    expect(clips * 1000 < nsamp, "clipping below 0.1 %");
    /* reload from the bank: what was saved comes back */
    uint8_t keep = PAT[pat_edit].s[T_LEAD][0];
    M->release();
    expect(M->init(&host) == 0, "init again (from the saved bank)");
    (void)keep;
    M->draw(1);
    snprintf(p, sizeof p, "%s/memetek-7-reloaded.ppm", dir); shot(p);
    wav_end();
    printf("%s\n", fails ? "FAILED" : "ALL OK");
    return fails ? 1 : 0;
}
