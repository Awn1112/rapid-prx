/* rapid.prx - R6S rapid fire. GoldHEN plugin. */

#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>

#include <orbis/libkernel.h>
#include <orbis/VideoOut.h>
#include <orbis/Pad.h>

#define LOG(...) sceKernelDebugOutText(0, __VA_ARGS__)

#define PAD_UP       0x0010
#define PAD_RIGHT    0x0020
#define PAD_DOWN     0x0040
#define PAD_LEFT     0x0080
#define PAD_R2       0x0200
#define PAD_CIRCLE   0x2000
#define PAD_CROSS    0x4000
#define PAD_SQUARE   0x8000

#define COMBO   (PAD_RIGHT | PAD_SQUARE)
#define CFG     "/data/rapid_prx.cfg"

static int      g_on;
static int      g_ms = 40;
static int      g_open;
static int      g_sel;
static int      g_pad = -1;
static uint32_t g_btn, g_btn_prev;

/* ---------------- module param (GoldHEN CRT) ---------------- */
__asm__(
    ".intel_syntax noprefix \n"
    ".align 0x8 \n"
    ".section \".data.sce_module_param\" \n"
    "_sceProcessParam: \n"
    "  .quad 0x18 \n"
    "  .quad 0x13C13F4BF \n"
    "  .quad 0x4508101 \n"
    ".att_syntax prefix \n"
);

__asm__(
    ".intel_syntax noprefix \n"
    ".align 0x8 \n"
    ".data \n"
    "__dso_handle: \n"
    "  .quad 0 \n"
    "_sceLibc: \n"
    "  .quad 0 \n"
    ".att_syntax prefix \n"
);

/* ---------------------------- video ---------------------------- */
typedef struct {
    int active;
    void *buf[8];
    void *cur;
    uint32_t w, h, pitch;
} Vo;

static Vo g_vo;
static int (*o_reg)(int, int, void * const*, int, const OrbisVideoOutBufferAttribute*);
static int (*o_flip)(int, int, uint32_t, int64_t);
static int (*o_pad)(int, OrbisPadData*, int);
static int (*o_padstate)(int, OrbisPadData*);

static inline void px(int x, int y, uint32_t c, uint8_t a)
{
    uint32_t *fb, d;
    int r, g, b;

    if (!g_vo.active || !g_vo.cur) return;
    if (x < 0 || y < 0 || x >= (int)g_vo.w || y >= (int)g_vo.h) return;

    fb = (uint32_t*)g_vo.cur + y * g_vo.pitch + x;
    d = *fb;
    r = (((c >> 16) & 0xFF) * a + ((d >> 16) & 0xFF) * (255 - a)) / 255;
    g = (((c >>  8) & 0xFF) * a + ((d >>  8) & 0xFF) * (255 - a)) / 255;
    b = (( c        & 0xFF) * a + ( d        & 0xFF) * (255 - a)) / 255;
    *fb = (r << 16) | (g << 8) | b;
}

static void rect(int x, int y, int w, int h, uint32_t c, uint8_t a)
{
    for (int j = 0; j < h; j++)
        for (int i = 0; i < w; i++) px(x + i, y + j, c, a);
}

static void box(int x, int y, int w, int h, uint32_t c, uint8_t a)
{
    rect(x, y, w, 1, c, a); rect(x, y + h - 1, w, 1, c, a);
    rect(x, y, 1, h, c, a); rect(x + w - 1, y, 1, h, c, a);
}

static const uint8_t FONT[64][5] = {
{0,0,0,0,0},{0,0,0x5F,0,0},{0,7,0,7,0},{0x14,0x7F,0x14,0x7F,0x14},
{0x24,0x2A,0x7F,0x2A,0x12},{0x23,0x13,8,0x64,0x62},{0x36,0x49,0x55,0x22,0x50},
{0,5,3,0,0},{0,0x1C,0x22,0x41,0},{0,0x41,0x22,0x1C,0},{0x14,8,0x3E,8,0x14},
{8,8,0x3E,8,8},{0,0x50,0x30,0,0},{8,8,8,8,8},{0,0x60,0x60,0,0},{0x20,0x10,8,4,2},
{0x3E,0x51,0x49,0x45,0x3E},{0,0x42,0x7F,0x40,0},{0x42,0x61,0x51,0x49,0x46},
{0x21,0x41,0x45,0x4B,0x31},{0x18,0x14,0x12,0x7F,0x10},{0x27,0x45,0x45,0x45,0x39},
{0x3C,0x4A,0x49,0x49,0x30},{1,0x71,9,5,3},{0x36,0x49,0x49,0x49,0x36},
{6,0x49,0x49,0x29,0x1E},{0,0x36,0x36,0,0},{0,0x56,0x36,0,0},{8,0x14,0x22,0x41,0},
{0x14,0x14,0x14,0x14,0x14},{0,0x41,0x22,0x14,8},{2,1,0x51,9,6},{0x32,0x49,0x79,0x41,0x3E},
{0x7E,0x11,0x11,0x11,0x7E},{0x7F,0x49,0x49,0x49,0x36},{0x3E,0x41,0x41,0x41,0x22},
{0x7F,0x41,0x41,0x22,0x1C},{0x7F,0x49,0x49,0x49,0x41},{0x7F,9,9,9,1},
{0x3E,0x41,0x49,0x49,0x7A},{0x7F,8,8,8,0x7F},{0,0x41,0x7F,0x41,0},
{0x20,0x40,0x41,0x3F,1},{0x7F,8,0x14,0x22,0x41},{0x7F,0x40,0x40,0x40,0x40},
{0x7F,2,0x0C,2,0x7F},{0x7F,4,8,0x10,0x7F},{0x3E,0x41,0x41,0x41,0x3E},
{0x7F,9,9,9,6},{0x3E,0x41,0x51,0x21,0x5E},{0x7F,9,0x19,0x29,0x46},
{0x46,0x49,0x49,0x49,0x31},{1,1,0x7F,1,1},{0x3F,0x40,0x40,0x40,0x3F},
{0x1F,0x20,0x40,0x20,0x1F},{0x3F,0x40,0x38,0x40,0x3F},{0x63,0x14,8,0x14,0x63},
{7,8,0x70,8,7},{0x61,0x51,0x49,0x45,0x43},{0,0x7F,0x41,0x41,0},
{2,4,8,0x10,0x20},{0,0x41,0x41,0x7F,0},{4,2,1,2,4},{0x40,0x40,0x40,0x40,0x40}
};

static void text(int x, int y, uint32_t c, const char *s)
{
    for (; *s; s++) {
        char ch = *s;
        const uint8_t *g;
        if (ch >= 'a' && ch <= 'z') ch -= 32;
        if (ch < 32 || ch > 95) ch = '?';
        g = FONT[ch - 32];
        for (int i = 0; i < 5; i++)
            for (int j = 0; j < 7; j++)
                if (g[i] & (1 << j)) px(x + i, y + j, c, 255);
        x += 6;
    }
}

static void textf(int x, int y, uint32_t c, const char *f, ...)
{
    char b[64];
    va_list v;
    va_start(v, f);
    vsnprintf(b, sizeof b, f, v);
    va_end(v);
    text(x, y, c, b);
}

/* ---------------------------- menu ----------------------------- */
static void menu_draw(void)
{
    rect(250, 150, 200, 80, 0x000000, 215);
    box(250, 150, 200, 80, 0x30C8FF, 255);
    rect(250, 150, 200, 18, 0x0A4A66, 255);
    text(258, 156, 0xFFFFFF, "RAPID FIRE");

    if (g_sel == 0) rect(254, 174, 192, 16, 0x0E5C7A, 255);
    text(258, 178, 0xFFFFFF, "ENABLE");
    text(410, 178, g_on ? 0x40FF40 : 0xFF5050, g_on ? "ON" : "OFF");

    if (g_sel == 1) rect(254, 194, 192, 16, 0x0E5C7A, 255);
    text(258, 198, 0xFFFFFF, "SPEED");
    textf(400, 198, 0xFFC040, "%d MS", g_ms);

    text(254, 218, 0x6FA8BC, "X TOGGLE  L/R ADJUST  O CLOSE");
}

static void menu_input(void)
{
    uint32_t pr = g_btn & ~g_btn_prev;

    if ((g_btn & COMBO) == COMBO && !(g_btn_prev & COMBO)) {
        g_open = !g_open;
        g_sel = 0;
        LOG("menu toggle");
    }
    if (!g_open) return;

    if (pr & (PAD_UP | PAD_DOWN)) g_sel = (g_sel + 1) % 2;
    if (pr & PAD_CIRCLE) g_open = 0;
    if ((pr & PAD_CROSS) && g_sel == 0) g_on = !g_on;

    if (g_sel == 1) {
        if (g_btn & PAD_LEFT)  { g_ms -= 2; if (g_ms < 10)  g_ms = 10; }
        if (g_btn & PAD_RIGHT) { g_ms += 2; if (g_ms > 200) g_ms = 200; }
    }
}

/* ---------------------------- hooks ---------------------------- */
static int h_reg(int vo, int start, void * const *bufs, int n,
                 const OrbisVideoOutBufferAttribute *a)
{
    int r = o_reg(vo, start, bufs, n, a);

    if (r == 0 && a) {
        g_vo.w = a->width;
        g_vo.h = a->height;
        g_vo.pitch = a->pixelPitch ? a->pixelPitch : a->width;
        for (int i = 0; i < n && i < 8; i++) g_vo.buf[i] = bufs[i];
        g_vo.active = 1;
        LOG("videoout %ux%u", g_vo.w, g_vo.h);
    }
    return r;
}

static int h_flip(int vo, int port, uint32_t frame, int64_t arg)
{
    if (g_vo.active && frame < 8 && g_vo.buf[frame]) {
        g_vo.cur = g_vo.buf[frame];

        if (g_open) {
            rect(0, 0, g_vo.w, g_vo.h, 0x000000, 80);
            menu_draw();
        } else {
            box(8, 8, 8, 8, g_on ? 0x40FF40 : 0x30C8FF, 200);
            if (g_on) textf(22, 9, 0x30C8FF, "%dMS", g_ms);
        }
    }
    return o_flip(vo, port, frame, arg);
}

static void process(OrbisPadData *d)
{
    g_btn_prev = g_btn;
    g_btn = d->buttons;

    menu_input();

    if (g_on && !g_open && (d->buttons & PAD_R2)) {
        uint64_t t = sceKernelGetProcessTime() / 1000ULL;
        if ((t / (uint64_t)g_ms) & 1ULL) d->buttons &= ~PAD_R2;
    }
}

static int h_pad(int h, OrbisPadData *d, int n)
{
    int r = o_pad(h, d, n);
    if (r > 0 && d) process(d);
    return r;
}

static int h_padstate(int h, OrbisPadData *d)
{
    int r = o_padstate(h, d);
    if (r == 0 && d) process(d);
    return r;
}

static void *stub(void *fn)
{
    static uint8_t pool[1024];
    static int u;
    uint8_t *p = pool + u;
    u += 16;
    p[0] = 0x48; p[1] = 0xB8;
    *(void**)(p + 2) = fn;
    p[10] = 0xFF; p[11] = 0xE0;
    return p;
}

static void patch(void **slot, void *target, void *repl)
{
    uintptr_t pg = (uintptr_t)target & ~0xFFFULL;
    uint8_t *p = (uint8_t*)target;

    sceKernelMprotect((void*)pg, 0x2000, 7);
    *slot = target;
    p[0] = 0x48; p[1] = 0xB8;
    *(uintptr_t*)(p + 2) = (uintptr_t)stub(repl);
    p[10] = 0xFF; p[11] = 0xE0;
    memset(p + 12, 0x90, 2);
}

static void cfg_load(void)
{
    FILE *f = fopen(CFG, "r");
    if (!f) return;
    fread(&g_on, sizeof g_on, 1, f);
    fread(&g_ms, sizeof g_ms, 1, f);
    fclose(f);
}

static void cfg_save(void)
{
    FILE *f = fopen(CFG, "w");
    if (!f) return;
    fwrite(&g_on, sizeof g_on, 1, f);
    fwrite(&g_ms, sizeof g_ms, 1, f);
    fclose(f);
}

/* --------------------------- lifecycle ------------------------- */
int module_start(size_t argc, const void *argv)
{
    LOG("rapid.prx start");
    cfg_load();

    g_pad = scePadGetHandle(0x1000000, 0, 0);
    LOG("pad handle %d", g_pad);

    patch((void**)&o_reg,      (void*)sceVideoOutRegisterBuffers, (void*)h_reg);
    patch((void**)&o_flip,     (void*)sceVideoOutSubmitFlip,      (void*)h_flip);
    patch((void**)&o_pad,      (void*)scePadRead,                 (void*)h_pad);
    patch((void**)&o_padstate, (void*)scePadReadState,            (void*)h_padstate);

    LOG("hooks installed");
    return 0;
}

int module_stop(size_t argc, const void *argv)
{
    cfg_save();
    if (g_pad >= 0) scePadClose(g_pad);
    LOG("rapid.prx stop");
    return 0;
}

int _init(size_t args, const void *argp)
{
    return module_start(args, argp);
}

int _fini(size_t args, const void *argp)
{
    return module_stop(args, argp);
}
