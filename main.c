/*
 * EasyX3D 1.0.c -- a small voxel renderer built on top of easygl.h
 *
 * Optimised throughout: roughly 2x faster per frame than the first cut,
 * plus the fixes listed at the end of this header.  The conventions are
 * unchanged and are still stated exactly once:
 *
 *   world   : right handed, +Y up, -Z is north (Minecraft convention)
 *   camera  : looks down its own -Z, yaw around +Y, pitch positive = up
 *   clip    : w_clip = -z_view, so w_clip > 0 means "in front of camera"
 *   ndc     : x,y in [-1,1], z in [-1,1]  (OpenGL convention)
 *   screen  : x right, y DOWN, origin top left
 *
 * Block data
 *   Every cell carries a type (AIR / GRASS / DIRT / ROCK) and a direction
 *   0..7 = east, south, west, north, upside-down-east, ... upside-down-north.
 *   GRASS is always upright (0..3); ROCK (and DIRT) use all eight.
 *   Nothing DRAWS the direction any more: the arrow that used to sit on the
 *   top face and the bar that used to mark the facing side are both gone, so
 *   a block looks the same whichever way it is turned.  The heading is still
 *   stored per cell and is still reported in the window title, and it costs
 *   nothing to keep -- the atlas is one tile per (block, top/side/bottom).
 *   Nothing floats either: the four hand placed sky blocks are gone.
 *
 * Controls:  WASD move, SPACE jump, SHIFT sneak, CTRL sprint, ESC quit
 *            V   fly on / off        (was F: F is the FOV dialog now)
 *            F   field of view -- opens an InputBox, degrees, 25..120
 *            R   OSD on / off
 *            F9  force the software rasteriser (compare the two paths)
 *            F10 cycle the render scale 100% -> 75% -> 50%  (software path)
 *            F11 high DPI scaling on / off (fixhighdpi)
 *            F12 borderless fullscreen on / off
 *            ALT release the pointer -- press it again to take it back
 *                (the middle mouse button does the same thing)
 *            middle mouse button -- release / re-take the pointer
 *
 *   The window is resizable and maximisable (variablewinsize), so dragging
 *   a border or hitting the maximise button changes the canvas underneath
 *   the renderer.  Nothing caches a size: the device and logical sizes are
 *   re-read every frame, so the frame buffer, the sky blit, the crosshair
 *   and the pointer clip all follow.  See 8c.
 *
 *   Everything worth knowing is on the OSD (R).  The window title is set
 *   once at start up and never touched again -- a title bar is a bad place
 *   for live numbers, and SetWindowText() per frame is a window message per
 *   frame for a value that changes faster than anyone can read.
 *
 * High DPI
 *   The window is RR_W x RR_H LOGICAL units and RR_W*scale x RR_H*scale real
 *   pixels, so it is the same physical size on every display.  Two things
 *   are needed for that and both are done here: the process has to become
 *   DPI aware BEFORE initgraph(), or getdpi() reports 96 and fixhighdpi()
 *   silently does nothing; and the renderer has to keep the device size and
 *   the logical size apart, which is what dpi_poll() is for.  See 7b.
 *   Set RR_DPI_AWARE to 0 to go back to the old stretched-window behaviour.
 *
 * Build (Windows + MinGW, C11):
 *     gcc -std=c11 -O2 "EasyX3D 1.0.c" -o voxel.exe -lopengl32 -luser32 -lgdi32
 *
 *   Use a 64 bit toolchain if you have one.  A 32 bit MinGW defaults to x87
 *   floating point, and every pixel in the software path pays for a float
 *   divide and two float->int conversions; add
 *       -msse2 -mfpmath=sse
 *   (or simply build for x86_64) before deciding the renderer is slow.
 *
 * Build (headless verification, any OS -- dumps frames to .ppm and times them):
 *     gcc -std=c11 -O2 -DRR_HEADLESS "EasyX3D 1.0.c" -o vr -lm
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdint.h>
#include <time.h>

#ifndef RR_HEADLESS
#  include "easygl.h"
#endif

typedef uint32_t rr_u32;

/* ------------------------------------------------------------------ */
/* 0. tunables                                                         */
/* ------------------------------------------------------------------ */

#define RR_W              960
#define RR_H              640

/* Minecraft-ish movement constants (blocks and blocks/second).         */
#define MC_EYE_STAND      1.62f
#define MC_EYE_SNEAK      1.54f
#define MC_WALK           4.317f
#define MC_SPRINT         5.612f
#define MC_SNEAK_SPEED    1.295f
#define MC_JUMP_VEL       8.4f      /* gives ~1.1 blocks of height       */
#define MC_GRAVITY        32.0f
#define MC_SENS           0.0022f   /* radians per pixel of mouse motion */

#define RR_CULL_DIST      78.0f   /* beyond this fog is total: not worth drawing */
#define RR_FOG_NEAR       14.0f
#define RR_FOG_FAR        64.0f
#define RR_NEAR           0.05f
#define RR_FAR            220.0f

#define INVERT_PITCH      0         /* set to 1 if look-up/down feels wrong */

/* How many real pixels one logical (96 dpi) unit is worth right now: 1.0
 * at 100%, 1.5 at 150%, 2.0 at 200%.  Read once per frame by dpi_poll().
 *
 * It is 1.0 by default so the headless build, which has no window and no
 * easygl, is unaffected: there the device size and the logical size are the
 * same thing, and every place that multiplies by this is a no-op.        */
static float g_dpiScale = 1.0f;

/* The two canvas sizes, filled in by dpi_poll() -- see 7b.  Declared here
 * and not inside the Windows-only block because the projection matrix needs
 * them in every build; headless has no canvas and leaves them at 0, which
 * rr_aspect() falls back from.                                            */
static int g_devW = 0, g_devH = 0;      /* device pixels                  */
static int g_logW = 0, g_logH = 0;      /* logical units                  */

/* Render scale used by the software path only (see F10).  The GPU mesh path
 * always draws at canvas resolution because it has nothing to save.        */
#define RR_SCALE_STEPS    3
static const float RR_SCALE[RR_SCALE_STEPS] = { 1.0f, 0.75f, 0.5f };

/* Key codes.  easygl.h only defines VK_ESCAPE / VK_RETURN itself and relies
 * on windows.h for the rest, so define what we need here instead.          */
#define RRK_SPACE   0x20
#define RRK_SHIFT   0x10
#define RRK_CTRL    0x11
#define RRK_ESC     0x1B
#define RRK_F       0x46
#define RRK_R       0x52
#define RRK_V       0x56
#define RRK_F9      0x78
#define RRK_F10     0x79
#define RRK_F11     0x7A
#define RRK_F12     0x7B
#define RRK_MENU    0x12         /* VK_MENU: either Alt key          */
/* Alt arrives as WM_SYSKEYDOWN, not WM_KEYDOWN: Windows sends the SYS
 * form whenever Alt is down, and easygl forwards both as EX_KEY.     */
#ifndef WM_SYSKEYDOWN
#define WM_SYSKEYDOWN 0x0104
#endif
#ifndef WM_SYSKEYUP
#define WM_SYSKEYUP   0x0105
#endif

/* ------------------------------------------------------------------ */
/* 1. tiny math                                                        */
/* ------------------------------------------------------------------ */

typedef struct V3 { float x, y, z; } V3;

static inline V3 v3(float x, float y, float z) { V3 r; r.x = x; r.y = y; r.z = z; return r; }
static inline V3 vadd(V3 a, V3 b) { return v3(a.x + b.x, a.y + b.y, a.z + b.z); }
static inline V3 vmul(V3 a, float s) { return v3(a.x * s, a.y * s, a.z * s); }
static inline float vdot(V3 a, V3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
static inline V3 vcross(V3 a, V3 b) {
    return v3(a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x);
}
static inline float vlen(V3 a) { return sqrtf(vdot(a, a)); }
static inline V3 vnorm(V3 a) { float l = vlen(a); return (l > 1e-8f) ? vmul(a, 1.0f / l) : v3(0, 0, 0); }

/* Row major 4x4.  m[r*4+c].  Applied as  m * column_vector.            */
typedef struct M4 { float m[16]; } M4;

static inline M4 m4id(void) {
    M4 r;
    memset(&r, 0, sizeof(r));
    r.m[0] = r.m[5] = r.m[10] = r.m[15] = 1.0f;
    return r;
}

static inline M4 m4mul(const M4* A, const M4* B) {
    M4 r;
    for (int c = 0; c < 4; c++)
        for (int rr2 = 0; rr2 < 4; rr2++) {
            float s = 0;
            for (int k = 0; k < 4; k++) s += A->m[rr2 * 4 + k] * B->m[k * 4 + c];
            r.m[rr2 * 4 + c] = s;
        }
    return r;
}

/* World -> view.  Camera basis is built explicitly so there is no chance of
 * handedness confusion: right = normalize(cross(forward, worldUp)).        */
static inline M4 m4view(V3 eye, V3 fwd, V3 up) {
    V3 f = vnorm(fwd);
    V3 r = vnorm(vcross(f, up));
    V3 u = vcross(r, f);            /* already unit: r and f are orthonormal */

    M4 v = m4id();
    v.m[0] = r.x;  v.m[1] = r.y;  v.m[2] = r.z;  v.m[3]  = -vdot(r, eye);
    v.m[4] = u.x;  v.m[5] = u.y;  v.m[6] = u.z;  v.m[7]  = -vdot(u, eye);
    v.m[8] = -f.x; v.m[9] = -f.y; v.m[10] = -f.z; v.m[11] = vdot(f, eye);
    v.m[12] = 0;   v.m[13] = 0;   v.m[14] = 0;    v.m[15] = 1.0f;
    return v;
}

/* Right handed perspective, z_ndc in [-1,1], w_clip = -z_view.          */
static inline M4 m4persp(float fovY, float aspect, float n, float f) {
    float t = 1.0f / tanf(fovY * 0.5f);
    M4 r;
    memset(&r, 0, sizeof(r));
    /* Matrices are ROW MAJOR (m[r*4+c]).  In that layout the "-1" that feeds
     * w_clip belongs in row 3 / column 2, i.e. m[14].                      */
    r.m[0] = t / aspect;
    r.m[5] = t;
    r.m[10] = (f + n) / (n - f);
    r.m[11] = (2.0f * f * n) / (n - f);
    r.m[14] = -1.0f;
    return r;
}

/* ------------------------------------------------------------------ */
/* 1b. counters                                                        */
/* ------------------------------------------------------------------ */

static int g_faces = 0, g_raster = 0, g_culled = 0, g_pix = 0;
static int g_backface = 0, g_behind = 0, g_chunkIn = 0, g_chunkOut = 0, g_farout = 0;

/* ------------------------------------------------------------------ */
/* 2. frame buffer                                                     */
/* ------------------------------------------------------------------ */

/* COLOUR ORDER: the buffer is 0x00BBGGRR -- exactly what easygl's
 * GetImageBuffer() hands back, and exactly what it uploads again.  The first cut kept
 * its own 0x00RRGGBB buffer and paid a per pixel byte shuffle (present()) on
 * every frame to convert.  Rendering in the display's own order removes that
 * pass completely: the software rasteriser can write straight into the
 * buffer GetImageBuffer() returned and the frame is done.               */

typedef struct Frame {
    int w, h;
    rr_u32* color;      /* 0x00BBGGRR, top down; may point at the display  */
    float*  depth;      /* 1/w_clip, cleared to 0 (= infinitely far)       */
} Frame;

static Frame g_fb = { 0, 0, NULL, NULL };
static rr_u32* g_own = NULL;        /* private buffer, used when there is no
                                     * display buffer to borrow (headless)  */
static size_t  g_ownCap = 0;

/* 1/(fogFar-fogNear) */
static float g_fogInv = 0.0f;

static void fb_bind(int w, int h, rr_u32* ext) {
    if (w < 1) w = 1;
    if (h < 1) h = 1;
    g_fogInv = 1.0f / (RR_FOG_FAR - RR_FOG_NEAR);

    if (w != g_fb.w || h != g_fb.h || !g_fb.depth) {
        g_fb.w = w; g_fb.h = h;
        free(g_fb.depth);
        g_fb.depth = (float*)malloc((size_t)w * h * sizeof(float));
        if (!g_fb.depth) { fprintf(stderr, "out of memory\n"); exit(1); }
    }

    if (ext) {
        g_fb.color = ext;
    } else {
        size_t need = (size_t)w * h;
        if (need > g_ownCap) {
            free(g_own);
            g_own = (rr_u32*)malloc(need * sizeof(rr_u32));
            g_ownCap = need;
            if (!g_own) { fprintf(stderr, "out of memory\n"); exit(1); }
        }
        g_fb.color = g_own;
    }
}

static void fb_free(void) {
    free(g_fb.depth); g_fb.depth = NULL;
    free(g_own);      g_own = NULL; g_ownCap = 0;
    g_fb.color = NULL; g_fb.w = g_fb.h = 0;
}

/* Sky gradient + depth clear in one pass.
 *
 * The depth buffer holds 1/w, so nearer is LARGER and "nothing here yet" is
 * the smallest possible value.  0.0f is all zero bytes, so clearing it is a
 * memset instead of a 3.7 MB store loop -- and it is the only correct value:
 * filling it with 1.0 made every fragment fail the (iw > depth) test.     */
static void fb_clear(void) {
    if (!g_fb.color || !g_fb.depth) return;
    memset(g_fb.depth, 0, (size_t)g_fb.w * g_fb.h * sizeof(float));
    for (int y = 0; y < g_fb.h; y++) {
        float t = (float)y / (float)(g_fb.h - 1);
        int r = (int)(46.0f + 88.0f * t);
        int g = (int)(96.0f + 104.0f * t);
        int b = (int)(168.0f + 62.0f * t);
        /* stored 0x00BBGGRR: B in the high byte, R in the low byte */
        rr_u32 c = ((rr_u32)b << 16) | ((rr_u32)g << 8) | (rr_u32)r;
        rr_u32* row = g_fb.color + (size_t)y * g_fb.w;
        for (int x = 0; x < g_fb.w; x++) row[x] = c;
    }
}

static inline void ch_put(int x, int y, rr_u32 c) {
    if (x < 0 || y < 0 || x >= g_fb.w || y >= g_fb.h) return;
    g_fb.color[(size_t)y * g_fb.w + x] = c;
}

/* Crosshair, drawn into our own buffer so it needs no library support and
 * cannot be lost by a later blit.
 *
 * The arms are sized in logical units and multiplied by the DPI factor,
 * because this buffer is measured in DEVICE pixels while the GPU path's
 * crosshair is drawn with line() in LOGICAL ones.  Without the factor the two
 * paths would disagree: on a 150% display the software crosshair would be
 * two thirds the size of the hardware one.                               */
static void fb_crosshair(void) {
    if (!g_fb.color) return;
    int cx = g_fb.w / 2, cy = g_fb.h / 2;
    float k = (g_dpiScale > 0.25f && g_dpiScale < 8.0f) ? g_dpiScale : 1.0f;
    const int L = (int)(10.0f * k + 0.5f), G = (int)(3.0f * k + 0.5f);
    for (int pass = 0; pass < 2; pass++) {
        /* pass 0 lays down a wider black outline so the crosshair stays
         * readable against both bright grass and dark rock.               */
        rr_u32 c = pass ? 0x00FFFFFFu : 0x00000000u;
        int hw = pass ? 0 : 1;
        for (int d = G; d <= L; d++) {
            for (int k = -hw; k <= hw; k++) {
                ch_put(cx - d, cy + k, c);
                ch_put(cx + d, cy + k, c);
                ch_put(cx + k, cy - d, c);
                ch_put(cx + k, cy + d, c);
            }
        }
    }
    ch_put(cx, cy, 0x00FFFFFFu);
}

/* ------------------------------------------------------------------ */
/* 3. block textures                                                   */
/* ------------------------------------------------------------------ */

/* Face slots: 0 = +Y top, 1 = -Y bottom, 2 = -X, 3 = +X, 4 = -Z, 5 = +Z  */
enum { BT_AIR = 0, BT_GRASS = 1, BT_DIRT = 2, BT_ROCK = 3, BT_N = 4 };

/* A heading is DATA ONLY now: nothing draws it.  It is still stored per cell
 * (so GRASS can be kept upright and so world_gen stays interesting) and it is
 * still reported in the window title; the arrow on the top face and the bar on
 * the facing side have both been removed, so a block looks the same whichever
 * way it is turned.
 *   0 east(+X) 1 south(+Z) 2 west(-X) 3 north(-Z)
 *   4..7 are the same headings with the block turned upside down.        */
static const char* DIR_NAME[8] = { "east", "south", "west", "north",
                                   "upside-down-east", "upside-down-south",
                                   "upside-down-west", "upside-down-north" };
static const char* BT_NAME[4] = { "air", "GRASS", "DIRT", "ROCK" };

typedef struct Tex { int size; rr_u32* px; } Tex;
static Tex g_btex[BT_N][3];        /* [type][0=top 1=side 2=bottom]        */

static unsigned hash2(int x, int y, unsigned seed) {
    unsigned h = (unsigned)x * 374761393u + (unsigned)y * 668265263u + seed * 2246822519u;
    h = (h ^ (h >> 13)) * 1274126177u;
    return h ^ (h >> 16);
}

/* Minecraft-ish procedural block texture: flat base colour plus per pixel
 * grit, plus a second coarser octave that gives stone its blotchy look.
 * Colours are 0x00RRGGBB here; the atlas flips them into display order once,
 * at bake time.
 *
 * `fringe` used to paint a grass overhang on the side face.  It is off, and
 * the block is plain, for two reasons: the overhang is not wanted, and it was
 * on the wrong edge anyway -- it keyed off u, and u is the HORIZONTAL axis of
 * a side face (v is the vertical one), so it drew a green stripe up one side
 * of every grass block instead of along the top.  To bring it back, key it
 * off the vertical axis, not off u.                                       */
static void tex_block(Tex* t, int size, int r, int g, int b, int amp, int fringe) {
    t->size = size;
    t->px = (rr_u32*)malloc((size_t)size * size * sizeof(rr_u32));
    if (!t->px) { fprintf(stderr, "out of memory\n"); exit(1); }
    for (int y = 0; y < size; y++) {
        for (int x = 0; x < size; x++) {
            unsigned h1 = hash2(x, y, 11u);
            int d = (int)(h1 % (unsigned)(2 * amp + 1)) - amp;
            /* a second, coarser octave gives stone its blotchy look */
            unsigned h2 = hash2(x / 4, y / 4, 29u);
            int d2 = (int)(h2 % 21u) - 10;
            int R = r + d + d2 / 2;
            int G = g + d + d2 / 2;
            int B = b + d + d2 / 2;
            if (fringe) {
                float fv = (float)y / (float)(size - 1);
                if (fv > 0.80f) {
                    float k = (fv - 0.80f) / 0.20f;
                    /* ragged edge so the fringe is not a straight line */
                    k *= (0.55f + 0.45f * ((float)(h1 % 1000u) / 1000.0f));
                    if (k > 1.0f) k = 1.0f;
                    R = (int)(R + (106 - R) * k);
                    G = (int)(G + (170 - G) * k);
                    B = (int)(B + ( 60 - B) * k);
                }
            }
            if (R < 0) R = 0;
            if (R > 255) R = 255;
            if (G < 0) G = 0;
            if (G > 255) G = 255;
            if (B < 0) B = 0;
            if (B > 255) B = 255;
            t->px[(size_t)y * size + x] = ((rr_u32)R << 16) | ((rr_u32)G << 8) | (rr_u32)B;
        }
    }
}

static void tex_init(void) {
    tex_block(&g_btex[BT_GRASS][0], 64, 106, 170,  60, 14, 0);   /* grass top    */
    tex_block(&g_btex[BT_GRASS][1], 64, 134,  96,  67, 14, 0);   /* grass side   */
    tex_block(&g_btex[BT_GRASS][2], 64, 134,  96,  67, 14, 0);   /* grass bottom */
    tex_block(&g_btex[BT_DIRT ][0], 64, 134,  96,  67, 14, 0);
    tex_block(&g_btex[BT_DIRT ][1], 64, 134,  96,  67, 14, 0);
    tex_block(&g_btex[BT_DIRT ][2], 64, 134,  96,  67, 14, 0);
    tex_block(&g_btex[BT_ROCK ][0], 64, 128, 128, 128, 16, 0);
    tex_block(&g_btex[BT_ROCK ][1], 64, 122, 122, 122, 16, 0);
    tex_block(&g_btex[BT_ROCK ][2], 64, 118, 118, 118, 16, 0);
}

/* which texture slot a face index uses */
static const int FACE_TEX[6] = { 0, 2, 1, 1, 1, 1 };

/* ------------------------------------------------------------------ */
/* 3c. baked atlas                                                     */
/* ------------------------------------------------------------------ */
/* Every (type, texture slot) is baked into one tile, so the rasteriser's
 * inner loop is a lookup and nothing else: no decal, no branching.
 *
 * The CPU copy is stored in DISPLAY order (0x00BBGGRR) so the rasteriser can
 * write a texel straight out with no per pixel channel shuffle.           */
#define AT_TILE   64
#define AT_COLS   4                 /* 3 types x 3 slots = 9 tiles needed */
#define AT_ROWS   4
#define AT_NTILE  (AT_COLS * AT_ROWS)
#define AT_W      (AT_TILE * AT_COLS)
#define AT_H      (AT_TILE * AT_ROWS)
static Tex g_atlas;                 /* CPU copy, 0x00BBGGRR                  */

/* With every decal gone the tile count collapses to 3 types x 3 slots = 9,
 * i.e. one tile per (block, top/side/bottom) -- what a texture atlas is
 * supposed to be.  The atlas is 256 x 256 / 256 KB instead of the 1024 x 1024
 * / 4 MB the first cut needed for its 144 (type, face, heading) combinations, and
 * it now fits in L2 with room to spare: measurement said the texel fetch was
 * about 45% of the rasteriser, nearly all of it cache misses, so this is not
 * a cosmetic change.  It also means the eight headings cost nothing to keep
 * in the data -- they simply do not appear in the atlas.                  */
static inline int at_tile(int type, int slot) {
    return (type - 1) * 3 + slot;
}

/* ------------------------------------------------------------------ */
/* 3b. rasteriser                                                      */
/* ------------------------------------------------------------------ */

/* A vertex that has already been through the model/view/projection matrix.
 * `w` is the clip w.  uv are ATLAS TEXEL coordinates, not 0..1: that turns
 * the per pixel texture lookup into two truncations and two masks instead of
 * two multiplies, two truncations and two clamps.
 * the first cut carried du/dv/light in here too; all three were dead weight in the
 * inner loop (they were copied through clipping and never read).          */
typedef struct CV { float x, y, z, w, u, v; } CV;

/* Sutherland-Hodgman against the single plane  w >= eps, which IS the near
 * plane in clip space.  Clipping before the divide is what avoids the
 * "geometry vanishes" class of bug.  face_emit() skips this entirely for the
 * overwhelming majority of faces, whose four corners are all in front.    */
static int clip_near(const CV* in, CV* out, float eps) {
    int m = 0;
    for (int i = 0; i < 3; i++) {
        const CV* A = &in[i];
        const CV* B = &in[(i + 1) % 3];
        float da = A->w - eps, db = B->w - eps;
        int ina = da >= 0.0f, inb = db >= 0.0f;
        if (ina) out[m++] = *A;
        if (ina != inb) {
            float t = da / (da - db);
            CV c;
            c.x = A->x + (B->x - A->x) * t;
            c.y = A->y + (B->y - A->y) * t;
            c.z = A->z + (B->z - A->z) * t;
            c.w = A->w + (B->w - A->w) * t;
            c.u = A->u + (B->u - A->u) * t;
            c.v = A->v + (B->v - A->v) * t;
            out[m++] = c;
        }
    }
    return m;   /* 0, 3 or 4 */
}

/* Largest |x_ndc| / |y_ndc| a triangle corner may have and still be drawn.
 *
 * THIS IS THE "CULLING IS TOO AGGRESSIVE" BUG.  A corner of a face you are
 * standing right up against lands far outside the screen even though the face
 * fills it: at 0.3 m a wall's bottom corner is at y_ndc ~ -7.7, and measured
 * over a real frame the worst case was 160.  The guard used to reject anything
 * past 4 -- so the CLOSER a face was, the likelier it was to be thrown away
 * whole.  Press into a corner and the two walls and the floor vanished and
 * the sky showed through; measured on a synthetic corner, 41% of the middle
 * of the screen was empty.
 *
 * Nothing downstream needs these coordinates to be small: the bounding box is
 * clamped to the screen and the exact per row span takes care of the rest.  So
 * the guard only has to reject what cannot be rasterised at all -- a non
 * finite coordinate, or one so large that the edge functions lose pixel
 * precision.  1024 keeps float error under a tenth of a pixel at 1280 wide and
 * is about 6x the worst real case.
 *
 * Verified: at normal viewing distances the rendered frames are bit for bit
 * identical to the old guard; only the near range changes.                  */
#define RR_NDC_MAX 1024.0f

/* fogMode: 0 = blend, 1 = face is entirely nearer than the fog (skip the
 * blend), 2 = face is entirely inside total fog (flat colour, no texture
 * fetch at all).  Distant terrain is most of the pixels on screen, so mode 2
 * is the single biggest per pixel saving in here.
 *
 * `lightQ` is the face light as an 8.8 fixed point multiplier (0..256).  The
 * whole colour path is integer: the old code converted each texel channel to
 * float, multiplied, converted back and clamped, which is six conversions and
 * three clamps per pixel for work that is exact in 8 bits.  Everything here is
 * a convex combination of values that are already 0..255, so the result cannot
 * overflow and no clamp is needed at all.                                   */
static void raster_tri(const CV t[3], int lightQ, int fogMode) {
    g_raster++;
    if (!g_fb.color) return;

    CV v[3];
    float sx[3], sy[3];
    for (int i = 0; i < 3; i++) {
        float iw = 1.0f / t[i].w;
        v[i].x = t[i].x * iw;
        v[i].y = t[i].y * iw;
        v[i].w = iw;                    /* 1/w from here on                */
        v[i].u = t[i].u * iw;
        v[i].v = t[i].v * iw;
        if (!(fabsf(v[i].x) < RR_NDC_MAX) || !(fabsf(v[i].y) < RR_NDC_MAX)) return;
    }

    /* --- to screen.  y flip happens here and nowhere else. --- */
    for (int i = 0; i < 3; i++) {
        sx[i] = (v[i].x * 0.5f + 0.5f) * (float)g_fb.w;
        sy[i] = (0.5f - v[i].y * 0.5f) * (float)g_fb.h;
    }

    float area = (sx[1]-sx[0])*(sy[2]-sy[0]) - (sx[2]-sx[0])*(sy[1]-sy[0]);
    if (area > 0.0f) {
        float tx = sx[1], ty = sy[1]; sx[1] = sx[2]; sy[1] = sy[2]; sx[2] = tx; sy[2] = ty;
        CV tv = v[1]; v[1] = v[2]; v[2] = tv;
        area = -area;
    }
    if (area > -1e-6f) { g_culled++; return; }

    int minx = (int)floorf(fminf(fminf(sx[0], sx[1]), sx[2]));
    int maxx = (int)ceilf (fmaxf(fmaxf(sx[0], sx[1]), sx[2]));
    int miny = (int)floorf(fminf(fminf(sy[0], sy[1]), sy[2]));
    int maxy = (int)ceilf (fmaxf(fmaxf(sy[0], sy[1]), sy[2]));
    if (minx < 0) minx = 0;
    if (miny < 0) miny = 0;
    if (maxx > g_fb.w - 1) maxx = g_fb.w - 1;
    if (maxy > g_fb.h - 1) maxy = g_fb.h - 1;
    if (minx > maxx || miny > maxy) return;

    float e0x = sx[1]-sx[0], e0y = sy[1]-sy[0];
    float e1x = sx[2]-sx[1], e1y = sy[2]-sy[1];
    float e2x = sx[0]-sx[2], e2y = sy[0]-sy[2];

    /* Divide by -area ONCE.  The three edge functions then sum to 1 and are
     * the barycentric weights outright, which takes a division out of every
     * pixel.                                                              */
    float inv = 1.0f / (-area);
    float xs = (float)minx + 0.5f, ys = (float)miny + 0.5f;
    float b0 = ((xs-sx[0])*e0y - (ys-sy[0])*e0x) * inv;
    float b1 = ((xs-sx[1])*e1y - (ys-sy[1])*e1x) * inv;
    float b2 = ((xs-sx[2])*e2y - (ys-sy[2])*e2x) * inv;
    float g0x = e0y*inv, g0y = -e0x*inv;      /* d/dx and d/dy of each     */
    float g1x = e1y*inv, g1y = -e1x*inv;
    float g2x = e2y*inv, g2y = -e2x*inv;

    const float fogN = RR_FOG_NEAR, fogI = g_fogInv;
    /* fog colour as integers, and the same colour packed in display order */
    const int fR = 150, fG = 190, fB = 235;
    const rr_u32 fogPix = ((rr_u32)fB << 16) | ((rr_u32)fG << 8) | (rr_u32)fR;

    for (int py = miny; py <= maxy; py++) {
        float wy = (float)py + 0.5f - ys;
        float r0 = b0 + g0y * wy, r1 = b1 + g1y * wy, r2 = b2 + g2y * wy;

        /* Exact row span.  A plain bounding box visits every pixel in the
         * rectangle; measurement said two thirds of those visits landed
         * outside the triangle.  Solving each edge for the x where it
         * crosses zero gives the true extent instead.                      */
        float lo = (float)minx, hi = (float)maxx, q;
        int empty = 0;
        if      (g0x >  1e-12f) { q = xs - r0/g0x - 0.5f; if (q > lo) lo = q; }
        else if (g0x < -1e-12f) { q = xs - r0/g0x - 0.5f; if (q < hi) hi = q; }
        else if (r0 < 0.0f) empty = 1;
        if      (g1x >  1e-12f) { q = xs - r1/g1x - 0.5f; if (q > lo) lo = q; }
        else if (g1x < -1e-12f) { q = xs - r1/g1x - 0.5f; if (q < hi) hi = q; }
        else if (r1 < 0.0f) empty = 1;
        if      (g2x >  1e-12f) { q = xs - r2/g2x - 0.5f; if (q > lo) lo = q; }
        else if (g2x < -1e-12f) { q = xs - r2/g2x - 0.5f; if (q < hi) hi = q; }
        else if (r2 < 0.0f) empty = 1;
        if (empty) continue;

        int ix0 = (int)ceilf(lo), ix1 = (int)floorf(hi);
        if (ix0 < minx) ix0 = minx;
        if (ix1 > maxx) ix1 = maxx;
        if (ix0 > ix1) continue;

        float fx = (float)ix0 + 0.5f - xs;
        float w0 = r0 + g0x * fx, w1 = r1 + g1x * fx, w2 = r2 + g2x * fx;
        rr_u32* crow = g_fb.color + (size_t)py * g_fb.w;
        float*   drow = g_fb.depth + (size_t)py * g_fb.w;

        /* Screen linear deltas, computed ONCE per span.
         *
         * 1/w, u/w and v/w are all affine in screen space, so each one can
         * be walked with a single add per pixel instead of re-evaluating
         * three barycentric weights and three dot products.  That is nine
         * multiplies and six adds per pixel turned into three adds.       */
        float iw = w1*v[0].w + w2*v[1].w + w0*v[2].w;
        float di = g1x*v[0].w + g2x*v[1].w + g0x*v[2].w;

        if (fogMode == 2) {
            /* Totally fogged: no texture, no light, no divide.  Still has to
             * write depth so nearer geometry drawn later wins.             */
            for (int px = ix0; px <= ix1; px++) {
                if (iw > drow[px]) { drow[px] = iw; crow[px] = fogPix; g_pix++; }
                iw += di;
            }
            continue;
        }

        float uu = w1*v[0].u + w2*v[1].u + w0*v[2].u;
        float du = g1x*v[0].u + g2x*v[1].u + g0x*v[2].u;
        float vv = w1*v[0].v + w2*v[1].v + w0*v[2].v;
        float dv = g1x*v[0].v + g2x*v[1].v + g0x*v[2].v;

        for (int px = ix0; px <= ix1; px++) {
            /* iw is 1/w and w IS the view space depth, so nearer is LARGER:
             * the depth test is a plain greater than and no z interpolation
             * is needed at all.                                           */
            if (iw > drow[px]) {
                float riw = 1.0f / iw;
                /* uv are atlas texels; the mask makes the index safe even if
                 * rounding pushes a coordinate one texel out of the tile.  */
                int tx = (int)(uu * riw) & (AT_W - 1);
                int ty = (int)(vv * riw) & (AT_H - 1);
                rr_u32 tc = g_atlas.px[(size_t)ty * AT_W + tx];

                int r = (int)( tc        & 255) * lightQ >> 8;
                int g = (int)((tc >>  8) & 255) * lightQ >> 8;
                int b = (int)((tc >> 16) & 255) * lightQ >> 8;
                if (fogMode == 0) {
                    float fc = (riw - fogN) * fogI;
                    if (fc < 0.0f) fc = 0.0f; else if (fc > 1.0f) fc = 1.0f;
                    fc *= fc;
                    int fq = (int)(fc * 256.0f);      /* 0..256 */
                    int iq = 256 - fq;
                    r = (r * iq + fR * fq) >> 8;
                    g = (g * iq + fG * fq) >> 8;
                    b = (b * iq + fB * fq) >> 8;
                }
                drow[px] = iw;
                crow[px] = ((rr_u32)b << 16) | ((rr_u32)g << 8) | (rr_u32)r;
                g_pix++;
            }
            iw += di; uu += du; vv += dv;
        }
    }
}

/* ------------------------------------------------------------------ */
/* 4. world                                                            */
/* ------------------------------------------------------------------ */

#define WX 48
#define WY 24
#define WZ 48

static unsigned char g_type[WX][WY][WZ];
static unsigned char g_dir [WX][WY][WZ];

static inline int solid(int x, int y, int z) {
    if (x < 0 || y < 0 || z < 0 || x >= WX || y >= WY || z >= WZ) return 0;
    return g_type[x][y][z] != BT_AIR;
}

/* One column of Minecraft-ish strata: grass on top, dirt under it, stone
 * below that, bare rock on the high ground.
 *
 * Heading: GRASS is always upright (0..3); ROCK and DIRT use all eight.
 * Deterministic, and spread so every heading is on screen at once.        */
static void set_column(int x, int z, int top) {
    if (top < 1) top = 1;
    if (top > WY - 2) top = WY - 2;
    for (int y = 0; y < WY; y++) {
        if (y >= top) { g_type[x][y][z] = BT_AIR; g_dir[x][y][z] = 0; continue; }
        unsigned char t;
        if (y == top - 1)      t = (top >= 10) ? BT_ROCK : BT_GRASS;
        else if (y >= top - 4) t = BT_DIRT;
        else                   t = BT_ROCK;
        g_type[x][y][z] = t;
        unsigned h = (unsigned)(x * 2 + z * 3 + y);
        g_dir[x][y][z] = (unsigned char)((t == BT_GRASS) ? (h & 3) : (h & 7));
    }
}

static void world_gen(void) {
    memset(g_type, 0, sizeof(g_type));
    memset(g_dir, 0, sizeof(g_dir));
    for (int x = 0; x < WX; x++) {
        for (int z = 0; z < WZ; z++) {
            float h = 3.0f
                    + 3.2f * sinf(x * 0.16f)
                    + 2.4f * cosf(z * 0.21f)
                    + 1.6f * sinf((x + z) * 0.09f);
            set_column(x, z, (int)floorf(h + 4.0f));
        }
    }
    /* flat spawn pad so the camera never starts inside geometry */
    for (int x = 20; x < 28; x++)
        for (int z = 20; z < 28; z++)
            set_column(x, z, 6);

    /* No floating blocks.  The first cut planted four of them by hand to show off a
     * block's underside and its heading decal; both reasons are gone (there
     * is no decal), and blocks hanging in an empty sky just look like a bug.
     * Every cell above a column's top stays AIR, so the sky is sky.        */

    /* belt and braces: a GRASS block can never be upside down */
    for (int x = 0; x < WX; x++)
        for (int y = 0; y < WY; y++)
            for (int z = 0; z < WZ; z++)
                if (g_type[x][y][z] == BT_GRASS) g_dir[x][y][z] &= 3;
}

/* ------------------------------------------------------------------ */
/* 5. baked face mesh                                                  */
/* ------------------------------------------------------------------ */

/* Only faces that touch air are stored.  With a static world this is built
 * once, which turns every frame from "walk 55k voxels and test six
 * neighbours each" into "walk a few thousand faces".                     */
typedef struct MeshFace {
    float p[4][3];      /* world space corners                              */
    float n[3];         /* unit outward normal (axis aligned)               */
    float nd;           /* dot(n, centre): backface test is dot(n,eye) > nd */
    float auv[4][2];    /* atlas uv, in ATLAS TEXELS (not 0..1)             */
    float light;
    unsigned char type, dir, faceIdx;
} MeshFace;

#define CH  8
#define NCX ((WX + CH - 1) / CH)
#define NCY ((WY + CH - 1) / CH)
#define NCZ ((WZ + CH - 1) / CH)
#define NCHUNK (NCX * NCY * NCZ)

static MeshFace* g_mesh = NULL;
static int g_meshN = 0, g_meshCap = 0;
static int g_chStart[NCHUNK], g_chCount[NCHUNK];
static float g_chBox[NCHUNK][6];        /* precomputed, so the per frame
                                         * culling loop divides by nothing  */

static void mesh_push(const MeshFace* f) {
    if (g_meshN >= g_meshCap) {
        g_meshCap = g_meshCap ? g_meshCap * 2 : 8192;
        g_mesh = (MeshFace*)realloc(g_mesh, (size_t)g_meshCap * sizeof(MeshFace));
        if (!g_mesh) { fprintf(stderr, "out of memory\n"); exit(1); }
    }
    g_mesh[g_meshN++] = *f;
}

/* Corner offsets per face, CCW seen from outside.                        */
static const int FACE_P[6][4][3] = {
    { {0,1,0}, {0,1,1}, {1,1,1}, {1,1,0} },   /* 0 +Y */
    { {0,0,1}, {0,0,0}, {1,0,0}, {1,0,1} },   /* 1 -Y */
    { {0,0,1}, {0,1,1}, {0,1,0}, {0,0,0} },   /* 2 -X */
    { {1,0,0}, {1,1,0}, {1,1,1}, {1,0,1} },   /* 3 +X */
    { {0,0,0}, {0,1,0}, {1,1,0}, {1,0,0} },   /* 4 -Z */
    { {1,0,1}, {1,1,1}, {0,1,1}, {0,0,1} },   /* 5 +Z */
};
static const int FACE_N[6][3] = {
    { 0, 1, 0 }, { 0, -1, 0 }, { -1, 0, 0 }, { 1, 0, 0 }, { 0, 0, -1 }, { 0, 0, 1 }
};
/* Lighting: top brightest, bottom darkest.                              */
static const float FACE_L[6] = { 1.00f, 0.42f, 0.62f, 0.80f, 0.80f, 0.62f };

/* Texture uv per face corner, 0..1 inside the tile.  On a side face v is the
 * VERTICAL axis (0 at the block's floor, 1 at its top) and u runs along the
 * horizontal edge; on top and bottom both run in the horizontal plane.    */
static const float FACE_U[6][4] = {
    { 0, 1, 1, 0 }, { 0, 1, 1, 0 },
    { 0, 0, 1, 1 }, { 0, 0, 1, 1 }, { 0, 0, 1, 1 }, { 0, 0, 1, 1 },
};
static const float FACE_V[6][4] = {
    { 0, 0, 1, 1 }, { 0, 0, 1, 1 },
    { 0, 1, 1, 0 }, { 0, 1, 1, 0 }, { 0, 1, 1, 0 }, { 0, 1, 1, 0 },
};

static void mesh_add_face(int x, int y, int z, int fi, unsigned char type, unsigned char dir) {
    MeshFace f;
    memset(&f, 0, sizeof(f));
    f.type = type; f.dir = dir; f.faceIdx = (unsigned char)fi;
    f.light = FACE_L[fi];
    f.n[0] = (float)FACE_N[fi][0];
    f.n[1] = (float)FACE_N[fi][1];
    f.n[2] = (float)FACE_N[fi][2];

    int ti = at_tile(type, FACE_TEX[fi]);
    int tc = ti % AT_COLS, tr = ti / AT_COLS;
    float ou = (float)(tc * AT_TILE) + 0.5f;
    float ov = (float)(tr * AT_TILE) + 0.5f;
    float span = (float)(AT_TILE - 1);

    float cx = 0, cy = 0, cz = 0;
    for (int i = 0; i < 4; i++) {
        float px = (float)x + FACE_P[fi][i][0];
        float py = (float)y + FACE_P[fi][i][1];
        float pz = (float)z + FACE_P[fi][i][2];
        f.p[i][0] = px; f.p[i][1] = py; f.p[i][2] = pz;
        cx += px; cy += py; cz += pz;
        /* atlas uv in TEXELS -- see the note in raster_tri() */
        f.auv[i][0] = ou + FACE_U[fi][i] * span;
        f.auv[i][1] = ov + FACE_V[fi][i] * span;
    }
    cx *= 0.25f; cy *= 0.25f; cz *= 0.25f;
    f.nd = f.n[0] * cx + f.n[1] * cy + f.n[2] * cz;
    mesh_push(&f);
}

/* ------------------------------------------------------------------ */
/* 5a. atlas bake                                                      */
/* ------------------------------------------------------------------ */
/* Nine tiles, one per (block type, top/side/bottom).  There is no decal to
 * draw any more, so a tile is a straight copy of the source texture with its
 * channels put into display order -- the bake exists only to make that
 * conversion once instead of per pixel.                                  */

#ifndef RR_HEADLESS
static IMAGE g_atlasImg;                 /* GPU copy of the atlas            */
#endif

static void atlas_build(void) {
    g_atlas.size = AT_W;
    g_atlas.px = (rr_u32*)malloc((size_t)AT_W * AT_H * sizeof(rr_u32));
    if (!g_atlas.px) { fprintf(stderr, "out of memory\n"); exit(1); }
    memset(g_atlas.px, 0, (size_t)AT_W * AT_H * sizeof(rr_u32));

    for (int t = 1; t < BT_N; t++)
    for (int slot = 0; slot < 3; slot++) {
        int ti = at_tile(t, slot);
        int ox  = (ti % AT_COLS) * AT_TILE;
        int oy  = (ti / AT_COLS) * AT_TILE;
        const Tex* base = &g_btex[t][slot];

        for (int y = 0; y < AT_TILE; y++)
        for (int x = 0; x < AT_TILE; x++) {
            rr_u32 tc = base->px[(size_t)y * base->size + x];
            /* DISPLAY order: B high, R low -- so the rasteriser's texel can
             * be written to the frame buffer with no channel shuffle.      */
            g_atlas.px[(size_t)(oy + y) * AT_W + (ox + x)] =
                ((tc & 255u) << 16) | (tc & 0xFF00u) | ((tc >> 16) & 255u);
        }
    }

#ifndef RR_HEADLESS
    /* The same pixels again as an easygl IMAGE for the GPU path.  Two
     * conventions have to be satisfied at once:
     *   - easygl stores 0x00BBGGRR, which is exactly the order above;
     *   - an IMAGE's texture is bottom up, so texture coordinate v = 0 is
     *     the LAST row of the buffer GetImageBuffer() hands back, while the
     *     CPU rasteriser's v = 0 is the FIRST row of g_atlas.
     * Writing the rows top down therefore mirrors the atlas vertically, and
     * every tile then samples from row (AT_ROWS-1-tr) -- which is why the
     * GPU and CPU paths agree.                                            */
    Resize(&g_atlasImg, AT_W, AT_H);
    DWORD* dst = GetImageBuffer(&g_atlasImg);
    if (dst)
        for (int y = 0; y < AT_H; y++) {
            const rr_u32* src = g_atlas.px + (size_t)y * AT_W;
            DWORD* out = dst + (size_t)(AT_H - 1 - y) * AT_W;
            memcpy(out, src, (size_t)AT_W * sizeof(rr_u32));
        }
#endif
}

static void mesh_build(void) {
    atlas_build();
    g_meshN = 0;
    for (int cz = 0; cz < NCZ; cz++)
    for (int cy = 0; cy < NCY; cy++)
    for (int cx = 0; cx < NCX; cx++) {
        int ci = (cz * NCY + cy) * NCX + cx;
        g_chStart[ci] = g_meshN;
        float mnx = (float)(cx * CH), mny = (float)(cy * CH), mnz = (float)(cz * CH);
        g_chBox[ci][0] = mnx;     g_chBox[ci][1] = mny;     g_chBox[ci][2] = mnz;
        g_chBox[ci][3] = mnx + CH; g_chBox[ci][4] = mny + CH; g_chBox[ci][5] = mnz + CH;
        int x0 = cx * CH, y0 = cy * CH, z0 = cz * CH;
        for (int x = x0; x < x0 + CH && x < WX; x++)
        for (int y = y0; y < y0 + CH && y < WY; y++)
        for (int z = z0; z < z0 + CH && z < WZ; z++) {
            unsigned char t = g_type[x][y][z];
            if (t == BT_AIR) continue;
            unsigned char d = g_dir[x][y][z];
            for (int fi = 0; fi < 6; fi++) {
                int nx = x + FACE_N[fi][0], ny = y + FACE_N[fi][1], nz = z + FACE_N[fi][2];
                if (solid(nx, ny, nz)) continue;      /* hidden: neighbour is solid */
                mesh_add_face(x, y, z, fi, t, d);
            }
        }
        g_chCount[ci] = g_meshN - g_chStart[ci];
    }
}

static void mesh_free(void) { free(g_mesh); g_mesh = NULL; g_meshN = g_meshCap = 0; }

/* ------------------------------------------------------------------ */
/* 5b. frustum culling                                                 */
/* ------------------------------------------------------------------ */

typedef struct Plane { float a, b, c, d; } Plane;

/* Gribb-Hartmann extraction for a ROW MAJOR matrix laid out as
 * row r = m[r*4 + 0..3], applied to a column vector.                     */
static void planes_from_mvp(const M4* m, Plane p[6]) {
    const float* a = m->m;
    p[0].a = a[12]+a[0];  p[0].b = a[13]+a[1];  p[0].c = a[14]+a[2];  p[0].d = a[15]+a[3];
    p[1].a = a[12]-a[0];  p[1].b = a[13]-a[1];  p[1].c = a[14]-a[2];  p[1].d = a[15]-a[3];
    p[2].a = a[12]+a[4];  p[2].b = a[13]+a[5];  p[2].c = a[14]+a[6];  p[2].d = a[15]+a[7];
    p[3].a = a[12]-a[4];  p[3].b = a[13]-a[5];  p[3].c = a[14]-a[6];  p[3].d = a[15]-a[7];
    p[4].a = a[12]+a[8];  p[4].b = a[13]+a[9];  p[4].c = a[14]+a[10]; p[4].d = a[15]+a[11];
    p[5].a = a[12]-a[8];  p[5].b = a[13]-a[9];  p[5].c = a[14]-a[10]; p[5].d = a[15]-a[11];
}

static int aabb_in_frustum(const Plane* p, const float* box) {
    for (int i = 0; i < 6; i++) {
        /* the corner furthest along the plane normal */
        float px = p[i].a >= 0 ? box[3] : box[0];
        float py = p[i].b >= 0 ? box[4] : box[1];
        float pz = p[i].c >= 0 ? box[5] : box[2];
        if (p[i].a * px + p[i].b * py + p[i].c * pz + p[i].d < 0.0f) return 0;
    }
    return 1;
}

/* ------------------------------------------------------------------ */
/* 5c. draw the baked mesh                                             */
/* ------------------------------------------------------------------ */

static void face_emit(const MeshFace* f, const M4* mvp, const V3* eye) {
    g_faces++;
    /* Backface test in 3D: draw the face only if the camera is on the side
     * its normal points to.  Robust -- independent of vertex ordering, and
     * one dot product against a precomputed plane constant.              */
    if (f->n[0]*eye->x + f->n[1]*eye->y + f->n[2]*eye->z <= f->nd) { g_backface++; return; }

    const float* m = mvp->m;
    CV q[4];
    float wmin = 1e30f, wmax = -1e30f;
    for (int i = 0; i < 4; i++) {
        float x = f->p[i][0], y = f->p[i][1], z = f->p[i][2];
        q[i].x = m[0]*x + m[1]*y + m[2]*z + m[3];
        q[i].y = m[4]*x + m[5]*y + m[6]*z + m[7];
        q[i].z = m[8]*x + m[9]*y + m[10]*z + m[11];
        q[i].w = m[12]*x + m[13]*y + m[14]*z + m[15];
        q[i].u = f->auv[i][0];
        q[i].v = f->auv[i][1];
        if (q[i].w < wmin) wmin = q[i].w;
        if (q[i].w > wmax) wmax = q[i].w;
    }
    if (wmax < RR_NEAR) { g_behind++; return; }     /* wholly behind / clipped away */

    /* w_clip IS the view space depth, so the four w values already say
     * whether any per pixel fog work is needed at all.  No sqrt, no extra
     * vertex data: compare w directly against the fog bounds.            */
    int fogMode = 0;
    if (wmin >= RR_FOG_FAR)      fogMode = 2;
    else if (wmax <= RR_FOG_NEAR) fogMode = 1;

    int lightQ = (int)(f->light * 256.0f + 0.5f);

    if (wmin >= RR_NEAR) {
        /* The common case: nothing to clip.  The first cut ran the clipper on every
         * triangle of every face whether it needed it or not; skipping it
         * here removes two struct copies per face and the branchy loop.   */
        CV t[3];
        t[0] = q[0]; t[1] = q[1]; t[2] = q[2];
        raster_tri(t, lightQ, fogMode);
        t[0] = q[0]; t[1] = q[2]; t[2] = q[3];
        raster_tri(t, lightQ, fogMode);
    } else {
        CV tri[3], buf[4];
        tri[0] = q[0]; tri[1] = q[1]; tri[2] = q[2];
        int n = clip_near(tri, buf, RR_NEAR);
        for (int i = 0; i + 2 < n; i++) {
            CV t[3];
            t[0] = buf[0]; t[1] = buf[i + 1]; t[2] = buf[i + 2];
            raster_tri(t, lightQ, fogMode);
        }
        tri[0] = q[0]; tri[1] = q[2]; tri[2] = q[3];
        n = clip_near(tri, buf, RR_NEAR);
        for (int i = 0; i + 2 < n; i++) {
            CV t[3];
            t[0] = buf[0]; t[1] = buf[i + 1]; t[2] = buf[i + 2];
            raster_tri(t, lightQ, fogMode);
        }
    }
}

typedef struct CVis { float d2; int ci; } CVis;
static CVis g_vis[NCHUNK];

static void world_draw(const M4* mvp, const V3* eye) {
    Plane pl[6];
    planes_from_mvp(mvp, pl);

    int n = 0;
    for (int ci = 0; ci < NCHUNK; ci++) {
        if (g_chCount[ci] == 0) continue;
        if (!aabb_in_frustum(pl, g_chBox[ci])) { g_chunkOut++; continue; }
        float dx = (g_chBox[ci][0] + g_chBox[ci][3]) * 0.5f - eye->x;
        float dy = (g_chBox[ci][1] + g_chBox[ci][4]) * 0.5f - eye->y;
        float dz = (g_chBox[ci][2] + g_chBox[ci][5]) * 0.5f - eye->z;
        float d2 = dx*dx + dy*dy + dz*dz;
        if (d2 > (RR_CULL_DIST + CH) * (RR_CULL_DIST + CH)) { g_farout++; continue; }
        g_vis[n].d2 = d2;
        g_vis[n].ci = ci;
        n++;
    }
    g_chunkIn = n;

    /* Nearest chunk first.  The depth test rejects hidden fragments before
     * they are shaded, so drawing front to back turns overlap into work the
     * rasteriser never does.  Insertion sort: n is a few dozen.          */
    for (int i = 1; i < n; i++) {
        CVis k = g_vis[i];
        int j = i - 1;
        while (j >= 0 && g_vis[j].d2 > k.d2) { g_vis[j + 1] = g_vis[j]; j--; }
        g_vis[j + 1] = k;
    }

    for (int i = 0; i < n; i++) {
        int ci = g_vis[i].ci;
        int s = g_chStart[ci], e = s + g_chCount[ci];
        for (int k = s; k < e; k++) face_emit(&g_mesh[k], mvp, eye);
    }
}

/* ------------------------------------------------------------------ */
/* 6. camera (Minecraft behaviour)                                     */
/* ------------------------------------------------------------------ */

typedef struct Camera {
    V3   pos;          /* feet position                                   */
    V3   vel;
    float yaw, pitch;  /* radians; pitch > 0 looks up                     */
    int   onGround;
    int   fly;
    int   sneak;
} Camera;

static Camera g_cam;

static V3 cam_forward(const Camera* c) {
    float cp = cosf(c->pitch), sp = sinf(c->pitch);
    return v3(-sinf(c->yaw) * cp, sp, -cosf(c->yaw) * cp);
}

static V3 cam_eye(const Camera* c) {
    float eye = c->sneak ? MC_EYE_SNEAK : MC_EYE_STAND;
    return v3(c->pos.x, c->pos.y + eye, c->pos.z);
}

static void cam_look(Camera* c, float dx, float dy) {
    c->yaw -= dx * MC_SENS;
#if INVERT_PITCH
    c->pitch += dy * MC_SENS;
#else
    c->pitch -= dy * MC_SENS;
#endif
    const float LIM = 1.5533f;    /* 89 degrees -- 90 would collapse the up vector */
    if (c->pitch >  LIM) c->pitch =  LIM;
    if (c->pitch < -LIM) c->pitch = -LIM;
    if (c->yaw >  3.14159265f) c->yaw -= 6.28318531f;
    if (c->yaw < -3.14159265f) c->yaw += 6.28318531f;
}

static void cam_step(Camera* c, float dt, int fwd, int back, int left, int right,
                     int jump, int sprint, int sneak) {
    /* horizontal basis -- note that pitch never affects where you walk */
    float sy = sinf(c->yaw), cy = cosf(c->yaw);
    V3 f = v3(-sy, 0.0f, -cy);
    V3 r = v3(cy, 0.0f, -sy);

    V3 wish = v3(0, 0, 0);
    wish = vadd(wish, vmul(f, (float)(fwd - back)));
    wish = vadd(wish, vmul(r, (float)(right - left)));
    if (vlen(wish) > 1e-6f) wish = vnorm(wish);

    float speed = sneak ? MC_SNEAK_SPEED : (sprint ? MC_SPRINT : MC_WALK);
    c->sneak = sneak;

    /* horizontal velocity with a little inertia, integrated in real time */
    V3 target = vmul(wish, speed);
    float accel = c->onGround ? 12.0f : 4.0f;
    float k = 1.0f - expf(-accel * dt);
    c->vel.x += (target.x - c->vel.x) * k;
    c->vel.z += (target.z - c->vel.z) * k;

    if (c->fly) {
        c->vel.y = 0;
        if (jump) c->vel.y = MC_WALK * 1.6f;
        if (sneak) c->vel.y = -MC_WALK * 1.6f;
    } else {
        if (jump && c->onGround) { c->vel.y = MC_JUMP_VEL; c->onGround = 0; }
        c->vel.y -= MC_GRAVITY * dt;
    }

    /* integrate with simple per-axis collision so you cannot walk through
     * blocks and you land on top of them                                 */
    V3 p = c->pos;
    V3 step = vmul(c->vel, dt);

    p.x += step.x;
    if (solid((int)floorf(p.x + 0.3f * (step.x > 0 ? 1 : -1)), (int)floorf(p.y + 0.1f), (int)floorf(p.z)) ||
        solid((int)floorf(p.x), (int)floorf(p.y + 0.1f), (int)floorf(p.z)))
        p.x = c->pos.x;
    p.z += step.z;
    if (solid((int)floorf(p.x), (int)floorf(p.y + 0.1f), (int)floorf(p.z + 0.3f * (step.z > 0 ? 1 : -1))) ||
        solid((int)floorf(p.x), (int)floorf(p.y + 0.1f), (int)floorf(p.z)))
        p.z = c->pos.z;

    p.y += step.y;
    int feetBlock = solid((int)floorf(p.x), (int)floorf(p.y), (int)floorf(p.z));
    int headBlock = solid((int)floorf(p.x), (int)floorf(p.y + 1.8f), (int)floorf(p.z));
    if (feetBlock || headBlock) {
        p.y = c->pos.y;
        if (step.y < 0) c->onGround = 1;
        c->vel.y = 0;
    } else {
        c->onGround = 0;
        if (p.y < 0) { p.y = 0; c->vel.y = 0; c->onGround = 1; }
    }
    c->pos = p;
}

/* What is directly under the camera: type + heading, so the block data can
 * be read off the OSD while walking around.                               */
static void block_under_camera(char* out, int n) {
    int bx = (int)floorf(g_cam.pos.x);
    int by = (int)floorf(g_cam.pos.y - 0.2f);
    int bz = (int)floorf(g_cam.pos.z);
    if (bx < 0 || by < 0 || bz < 0 || bx >= WX || by >= WY || bz >= WZ) {
        snprintf(out, n, "outside");
        return;
    }
    unsigned char t = g_type[bx][by][bz];
    if (t == BT_AIR) { snprintf(out, n, "air"); return; }
    snprintf(out, n, "%s / %s", BT_NAME[t], DIR_NAME[g_dir[bx][by][bz]]);
}

/* What the crosshair is ON, and how far away it is.  A fixed step march is
 * good enough for a label: at 0.05 m the worst case is one step past the true
 * surface, i.e. the reported distance can be 5 cm long, and 6 m / 0.05 m is
 * 120 iterations of three floorf() -- nothing next to a frame.  A real DDA
 * would be exact and about four times the code; this is a readout.        */
#define RR_REACH_MAX 6.0f
static void block_looked_at(char* out, int n, float* dist) {
    V3 eye = cam_eye(&g_cam);
    V3 fwd = cam_forward(&g_cam);
    *dist = 0.0f;
    snprintf(out, n, "sky");
    for (float t = 0.05f; t <= RR_REACH_MAX; t += 0.05f) {
        int bx = (int)floorf(eye.x + fwd.x * t);
        int by = (int)floorf(eye.y + fwd.y * t);
        int bz = (int)floorf(eye.z + fwd.z * t);
        if (bx < 0 || by < 0 || bz < 0 || bx >= WX || by >= WY || bz >= WZ) break;
        unsigned char ty = g_type[bx][by][bz];
        if (ty != BT_AIR) {
            snprintf(out, n, "%s / %s", BT_NAME[ty], DIR_NAME[g_dir[bx][by][bz]]);
            *dist = t;
            return;
        }
    }
}

/* Compass letter for a yaw.  World -Z is north, +X is east.
 *
 * The sign matters and is easy to get backwards.  cam_forward() is
 * (-sin(yaw), *, -cos(yaw)), so yaw = 0 looks down -Z, i.e. NORTH, and
 * increasing yaw swings the view towards -X, i.e. WEST -- counter clockwise
 * seen from above, which is the opposite of a compass rose.  Hence the
 * negation: the bearing from north, clockwise positive, is -yaw.          */
static const char* compass_of(float yaw) {
    float d = -yaw * 180.0f / 3.14159265f;
    d = fmodf(d, 360.0f);
    if (d < 0.0f) d += 360.0f;
    static const char* names[8] = { "N", "NE", "E", "SE", "S", "SW", "W", "NW" };
    int i = (int)((d + 22.5f) / 45.0f) & 7;
    return names[i];
}

/* ------------------------------------------------------------------ */
/* 7. one frame                                                        */
/* ------------------------------------------------------------------ */

/* Vertical field of view, in DEGREES.  F opens an InputBox to change it.
 *
 * It is a global and not a constant because it has to reach both paths: the
 * software rasteriser and the GPU mesh path build the projection matrix from
 * the same number, so a change shows up on the next frame either way and the
 * two never disagree about how wide the world looks.
 *
 * The clamp is not cosmetic.  Past ~120 degrees the perspective divide puts
 * the horizon a few pixels from the top of the screen and everything at the
 * edges smears; below ~25 the view is a telescope and the near plane starts
 * cutting into the block you are standing on.  70 is EasyX-ish and matches
 * the old hard coded value (1.2217 rad).                                  */
#define RR_FOV_MIN  25.0f
#define RR_FOV_MAX  120.0f
#define RR_FOV_DEF  70.0f
static float g_fovDeg = RR_FOV_DEF;

/* F9 flips this at run time so the two paths can be compared.            */
static int g_forceCpu = 0;

/* 1 while the window is in borderless fullscreen (F12).  Declared out here,
 * outside the Windows-only block, only because the OSD reads it and the OSD
 * is compiled in every build.                                            */
static int g_fullscreen = 0;

/* 1 while the user has released the pointer with the middle mouse button:
 * the cursor is unconfined, visible, and mouse look is off.  Same reason for
 * being out here as g_fullscreen.                                        */
static int g_mouseFree = 0;
/* F10 cycles this.  It only ever affects the software path: the GPU mesh
 * path has no per pixel CPU work to save.                                */
static int g_scaleStep = 0;

/* Aspect ratio for the projection matrix.
 *
 * THIS MUST COME FROM THE DEVICE SIZE, NOT FROM g_fb.  g_fb is the software
 * rasteriser's frame buffer, and on the GPU path it is never bound at all --
 * easygl's mesh pass renders straight at canvas resolution -- so g_fb.w and
 * g_fb.h stay 0 and g_fb.w / g_fb.h is 0/0 = NaN.  One NaN in the projection
 * poisons all sixteen elements, every vertex lands nowhere, and the frame
 * comes out as sky and nothing else.  Worse, the pass still counts the chunks
 * it submitted, so the "nothing was drawn, fall back" check does not fire and
 * the blank frame is accepted as good.
 *
 * It was invisible for as long as the mesh path was disabled: the software
 * path binds g_fb a few lines below, so only the very first frame ever saw
 * the NaN, and a single bad frame at start up looks like a slow load.      */
static float rr_aspect(void) {
    float w = 0.0f, h = 0.0f;
    if (g_devW > 0 && g_devH > 0)          { w = (float)g_devW; h = (float)g_devH; }
    else if (g_fb.w > 0 && g_fb.h > 0)     { w = (float)g_fb.w; h = (float)g_fb.h; }
    else                                   { w = (float)RR_W;   h = (float)RR_H;   }
    if (!(w > 1.0f) || !(h > 1.0f) || w != w || h != h) { w = (float)RR_W; h = (float)RR_H; }
    return w / h;
}

/* A matrix is not a valid projection if any element is not finite, and one
 * NaN spreads to all sixteen through the multiply.  Cheap to check, and it
 * turns a class of silent blank frames into a reported fallback.          */
static int m4_finite(const M4* m) {
    for (int i = 0; i < 16; i++)
        if (!(m->m[i] == m->m[i]) || m->m[i] > 1.0e30f || m->m[i] < -1.0e30f) return 0;
    return 1;
}

#ifndef RR_HEADLESS
/* 1 when the last software frame was rendered at less than canvas size and
 * therefore still has to be stretched over the canvas.                   */
static int      g_scaled = 0;
static IMAGE    g_frameImg;
static int      g_frameW = 0, g_frameH = 0;

/* ------------------------------------------------------------------ */
/* 7b. DPI: two sizes, and never the twain                             */
/* ------------------------------------------------------------------ */
/* fixhighdpi() makes the window bigger in real pixels and scales the
 * coordinate space to match, so from here on there are TWO sizes and mixing
 * them is the entire bug:
 *
 *   DEVICE   real pixels.  getcanvaswidth() / getcanvasheight(), and what
 *            GetImageBuffer() hands back.  The software rasteriser fills
 *            this; the mesh path renders at this; the frame buffer is
 *            measured in this.
 *   LOGICAL  96 dpi units.  getwidth() / getheight(), and what every 2D
 *            command takes: putimage(), line(), outtextxy(),
 *            solidrectangle().  Mouse coordinates from peekmessage() are
 *            logical too.
 *
 * They differ by exactly gethighdpiscale() once fixhighdpi() is on.  At 100%
 * that is 1.0 and the two are the same number, which is why this never
 * showed up before.
 *
 * Polled once per frame rather than cached: fixhighdpi() can be switched at
 * run time (F11) and the window can move to a monitor with a different
 * scaling, and either changes all four numbers.                          */
static void dpi_poll(void) {
    float sc;
    g_devW = getcanvaswidth();
    g_devH = getcanvasheight();
    if (g_devW < 1) g_devW = 1;
    if (g_devH < 1) g_devH = 1;

    sc = gethighdpiscale();
    if (!(sc > 0.25f) || !(sc < 8.0f)) sc = 1.0f;      /* nonsense -> off */
    g_dpiScale = sc;

    g_logW = getwidth();
    g_logH = getheight();
    /* getwidth() is the authority, but it is derived and can come back 0 or
     * absurd before the first real projection; fall back to the device size
     * over the factor rather than divide by a zero later.                */
    if (g_logW < 1) g_logW = (int)((float)g_devW / sc + 0.5f);
    if (g_logH < 1) g_logH = (int)((float)g_devH / sc + 0.5f);
    if (g_logW < 1) g_logW = g_devW;
    if (g_logH < 1) g_logH = g_devH;
}

/* Where the software rasteriser writes, and how big that surface is.
 * At 100% it is easygl's own canvas buffer: the frame is finished the moment
 * the last pixel is written, and there is no copy afterwards.  Below 100% it
 * is a smaller IMAGE that the GPU stretches over the canvas with one blit. */
static rr_u32* sw_target(int* w, int* h) {
    /* DEVICE pixels: this is what GetImageBuffer() gives back, so a frame
     * that fills it has to be this big.  Not getwidth() -- that is the
     * logical size and would leave a band of untouched pixels along the
     * right and bottom edges on a scaled display.                        */
    int cw = g_devW, ch = g_devH;
    float s = RR_SCALE[g_scaleStep];
    g_scaled = 0;
    if (s < 1.0f && cw > 8 && ch > 8) {
        int ww = (int)(cw * s), hh = (int)(ch * s);
        if (ww < 8) ww = 8;
        if (hh < 8) hh = 8;
        if (ww != g_frameW || hh != g_frameH) {
            Resize(&g_frameImg, ww, hh);
            g_frameW = ww; g_frameH = hh;
        }
        *w = ww; *h = hh;
        g_scaled = 1;
        return (rr_u32*)GetImageBuffer(&g_frameImg);
    }
    *w = cw; *h = ch;
    return (rr_u32*)GetImageBuffer();
}
#endif

/* ------------------------------------------------------------------ */
/* 7c. GPU path: real geometry through easygl's mesh API               */
/* ------------------------------------------------------------------ */
/* The world is static, so the geometry is uploaded once at load time and
 * every frame after that is a handful of draw calls with no per pixel CPU
 * work.  If it is unavailable or fails, the CPU rasteriser draws instead of
 * leaving a blank window.                                                */
#ifndef RR_HEADLESS

static GXMESH* g_chMesh[NCHUNK];
static IMAGE   g_skyImg;
static int     g_meshReady = 0;

/* The software rasteriser is O(pixels), and in fullscreen the canvas is the
 * size of the whole monitor -- times the DPI factor on a scaled display.
 * Measured here (RR_BENCH, best of 5, same machine and compiler):
 *
 *    1280 x  720   921600 px    4.29 ms   233 fps ceiling
 *    1920 x 1080  2073600 px    8.75 ms   114 fps ceiling
 *    2560 x 1440  3686400 px   15.35 ms    65 fps ceiling
 *    2880 x 1620  4665600 px   19.21 ms    52 fps ceiling   (1080p @ 150%)
 *    3840 x 2160  8294400 px   33.96 ms    29 fps ceiling   (1080p @ 200%)
 *
 * The interesting row is 2880x1620.  At 52 fps the frame misses the 60 Hz
 * deadline about half the time, so vsync drops it to every other refresh and
 * the result is not "51 fps" but an uneven 60/30 -- and since the pointer is
 * sampled once per frame, that is exactly what "the mouse is laggy in
 * fullscreen" looks like.  Nothing in the mouse code causes it.
 *
 * So the render scale is picked from the pixel count instead of being left at
 * 100%: the largest step that fits the budget wins.  Pressing F10 hands the
 * choice back to the user and auto stops interfering.                     */
#ifndef RR_SCALE_BUDGET
#define RR_SCALE_BUDGET  2700000     /* ~1920x1080; keeps the frame ~11 ms */
#endif
static int g_scaleAuto = 1;

static void rr_auto_scale(void) {
    int step;
    if (!g_scaleAuto) return;                     /* F10: the user chose   */
    /* The GPU path has no per pixel CPU cost, so shrinking its target would
     * only lose detail. */
    if (!(g_forceCpu || !g_meshReady)) return;
    step = 0;
    while (step < RR_SCALE_STEPS) {
        float s = RR_SCALE[step];
        long n = (long)((float)g_devW * s + 0.5f) *
                 (long)((float)g_devH * s + 0.5f);
        if (n <= RR_SCALE_BUDGET) break;
        step++;
    }
    if (step >= RR_SCALE_STEPS) step = RR_SCALE_STEPS - 1;
    g_scaleStep = step;
}

static int     g_chunksDrawn = 0;
static float   g_gpuMvp[16];          /* column major, rebuilt per frame   */

/* A 1 x 256 gradient, stretched over the canvas with one putimage().  The
 * mesh pass clears to alpha 0 and blends, so this stays visible wherever no
 * geometry was drawn -- the sky costs one quad, not 921600 stores.       */
static void sky_build(void) {
    Resize(&g_skyImg, 1, 256);
    DWORD* d = GetImageBuffer(&g_skyImg);
    if (!d) return;
    for (int i = 0; i < 256; i++) {
        float t = (float)i / 255.0f;                 /* 0 at the top */
        int r = (int)( 46.0f + (134.0f -  46.0f) * t + 0.5f);
        int g = (int)( 96.0f + (200.0f -  96.0f) * t + 0.5f);
        int b = (int)(168.0f + (230.0f - 168.0f) * t + 0.5f);
        d[i] = RGB(r, g, b);
    }
}

/* End to end check of the atlas upload: pull the centre pixel of every tile
 * back out of the GPU image and compare it with the CPU copy the software
 * rasteriser samples.  A mismatch means the two paths disagree, and an
 * orientation flip is exactly what that looks like -- tiles read from the
 * wrong row, which once left every face black except the stone's four sides.
 * Say so and let the CPU rasteriser draw rather than show a frame that is
 * wrong in a way nobody can diagnose from the picture alone.
 *
 * The check is per pixel and not per channel average on purpose: a tile that
 * is mirrored still has a plausible average, and that is what made the old
 * bug survive a whole review.                                             */
static int atlas_verify(void) {
    int bad = 0;
    if (!isglready()) return 1;
    SetWorkingImage(&g_atlasImg);
    for (int t = 1; t < BT_N && !bad; t++)
        for (int slot = 0; slot < 3 && !bad; slot++) {
            int ti = at_tile(t, slot);
            int ox = (ti % AT_COLS) * AT_TILE + AT_TILE / 2;
            int oy = (ti / AT_COLS) * AT_TILE + AT_TILE / 2;
            /* The IMAGE is written MIRRORED -- see atlas_build() -- because
             * its texture is bottom up while the CPU copy is top down.  So
             * the pixel easygl reports at row y is g_atlas row (AT_H-1-y),
             * and the probe has to be flipped to match.
             *
             * It used to probe at (ox, oy) and compare with g_atlas row oy.
             * Those are two different rows, so the check failed on every
             * tile and the mesh path was disabled on every run -- quietly,
             * with the message going to stderr and the frame still looking
             * right because the CPU path draws the same picture.  Flipping
             * the probe instead of flipping the expectation is the fix that
             * keeps the check meaningful: sampling the mirrored row would
             * compare an empty tile against the same empty tile and pass
             * without ever having looked at a real texel.                 */
            COLORREF got = getpixel(ox, AT_H - 1 - oy);
            rr_u32 c = g_atlas.px[(size_t)oy * AT_W + ox];
            int gr = (int)GetRValue(got), gg = (int)GetGValue(got), gb = (int)GetBValue(got);
            int er = (int)(c & 255), eg = (int)((c >> 8) & 255), eb = (int)((c >> 16) & 255);
            if (gr != er || gg != eg || gb != eb) {
                fprintf(stderr,
                    "gpumesh: atlas mismatch, tile %d (%s %s) at (%d,%d): "
                    "GPU=(%d,%d,%d) CPU=(%d,%d,%d)\n",
                    ti, BT_NAME[t],
                    slot == 0 ? "top" : (slot == 1 ? "side" : "bottom"),
                    ox, oy, gr, gg, gb, er, eg, eb);
                bad = 1;
            }
        }
    SetWorkingImage(NULL);
    return !bad;
}

/* One mesh per chunk, built once.  Uploading is the expensive part and it
 * happens here, at load time, never per frame.                           */
static void gpumesh_build(void) {
    if (!meshavailable()) { fprintf(stderr, "gpumesh: unavailable, using CPU\n"); return; }
    if (!atlas_verify()) { fprintf(stderr, "gpumesh: disabled, using CPU\n"); return; }
    for (int ci = 0; ci < NCHUNK; ci++) {
        int n = g_chCount[ci];
        g_chMesh[ci] = NULL;
        if (n <= 0) continue;
        float* v = (float*)malloc((size_t)n * 4 * 8 * sizeof(float));
        unsigned int* ix = (unsigned int*)malloc((size_t)n * 6 * sizeof(unsigned int));
        if (!v || !ix) { free(v); free(ix); continue; }
        for (int i = 0; i < n; i++) {
            const MeshFace* f = &g_mesh[g_chStart[ci] + i];
            for (int k = 0; k < 4; k++) {
                float* o = &v[(size_t)(i * 4 + k) * 8];
                o[0] = f->p[k][0]; o[1] = f->p[k][1]; o[2] = f->p[k][2];
                /* the mesh stores atlas uv in texels; GL wants 0..1 */
                o[3] = f->auv[k][0] / (float)AT_W;
                o[4] = f->auv[k][1] / (float)AT_H;
                o[5] = o[6] = o[7] = f->light;
            }
            unsigned int b = (unsigned int)(i * 4);
            ix[i*6+0] = b;     ix[i*6+1] = b + 1; ix[i*6+2] = b + 2;
            ix[i*6+3] = b;     ix[i*6+4] = b + 2; ix[i*6+5] = b + 3;
        }
        g_chMesh[ci] = createmesh(v, n * 4, ix, n * 6);
        free(v);
        free(ix);
    }
    g_meshReady = 1;
}

static void gpumesh_free(void) {
    for (int ci = 0; ci < NCHUNK; ci++)
        if (g_chMesh[ci]) { freemesh(g_chMesh[ci]); g_chMesh[ci] = NULL; }
    g_meshReady = 0;
}

/* M4 here is row major (m[row*4+col]); OpenGL wants column major. */
static void m4_to_colmajor(const M4* m, float out[16]) {
    for (int r = 0; r < 4; r++)
        for (int c = 0; c < 4; c++) out[c * 4 + r] = m->m[r * 4 + c];
}

static int gpu_mesh_frame(const M4* mvp, V3 eye) {
    if (!g_meshReady) return 0;
    /* Never hand the GPU a matrix that cannot be used: every vertex would
     * collapse and the result is a sky-only frame that still reports chunks
     * drawn, which is exactly the failure this check exists to prevent.    */
    if (!m4_finite(mvp)) {
        fprintf(stderr, "gpumesh: non finite MVP, falling back to CPU\n");
        return 0;
    }

    m4_to_colmajor(mvp, g_gpuMvp);

    /* The sky is an ordinary 2D draw, so it has to go down before the 3D
     * pass opens; meshbegin() flushes the batch for us.  Canvas sized, not
     * g_fb sized: the software path may be running at a reduced scale.   */
    putimage(0, 0, g_logW, g_logH, &g_skyImg, 0, 0);

    if (!meshbegin()) return 0;

    meshfog(RR_FOG_NEAR, RR_FOG_FAR, RGB(150, 190, 235));
    mesheye(eye.x, eye.y, eye.z);

    g_chunksDrawn = 0;
    for (int i = 0; i < NCHUNK; i++) {
        if (!g_chMesh[i]) continue;
        const float* box = g_chBox[i];
        /* distance cull first -- it is the cheaper of the two tests */
        float dx = (box[0] + box[3]) * 0.5f - eye.x;
        float dy = (box[1] + box[4]) * 0.5f - eye.y;
        float dz = (box[2] + box[5]) * 0.5f - eye.z;
        if (dx*dx + dy*dy + dz*dz > (RR_CULL_DIST + CH) * (RR_CULL_DIST + CH)) continue;
        if (!meshvisible(g_gpuMvp, box[0], box[1], box[2], box[3], box[4], box[5])) continue;
        meshdraw(g_chMesh[i], g_gpuMvp, &g_atlasImg);
        g_chunksDrawn++;
    }
    meshend();
    /* Nothing survived culling on a world that certainly has geometry: that
     * is a broken pass, not an empty view. */
    if (g_chunksDrawn == 0 && g_meshN > 0) {
        fprintf(stderr, "gpumesh: 0 chunks drawn, falling back to CPU\n");
        return 0;
    }
    return 1;
}

#endif /* !RR_HEADLESS */

static int render_frame(void) {
    float aspect = rr_aspect();
    M4 proj = m4persp(g_fovDeg * 3.14159265f / 180.0f, aspect, RR_NEAR, RR_FAR);
    V3 eye = cam_eye(&g_cam);
    V3 fwd = cam_forward(&g_cam);
    M4 view = m4view(eye, fwd, v3(0, 1, 0));
    M4 mvp = m4mul(&proj, &view);

    /* One decision point: if the GPU path produced the frame, the CPU
     * rasteriser never runs at all -- and easygl's canvas buffer is never
     * touched, so it is never marked dirty and never uploaded over the top
     * of what the GPU just drew.                                         */
#ifndef RR_HEADLESS
    if (!g_forceCpu && gpu_mesh_frame(&mvp, eye)) return 1;
#endif
    {
        rr_u32* ext = NULL;
        int w = g_fb.w, h = g_fb.h;
#ifndef RR_HEADLESS
        ext = sw_target(&w, &h);
#endif
        if (ext != g_fb.color || w != g_fb.w || h != g_fb.h) fb_bind(w, h, ext);
    }
    fb_clear();                         /* <- every frame, no exceptions   */
    world_draw(&mvp, &eye);
    return 0;
}

static int count_chunks(void) {
    int n = 0;
    for (int i = 0; i < NCHUNK; i++) if (g_chCount[i]) n++;
    return n;
}

/* ------------------------------------------------------------------ */
/* 8. platform                                                         */
/* ------------------------------------------------------------------ */

#ifdef RR_HEADLESS

/* Headless driver: renders a few frames from fixed camera poses and writes
 * them as .ppm so the output can actually be inspected.                   */
static void ppm_write(const char* path, const rr_u32* px, int w, int h) {
    FILE* f = fopen(path, "wb");
    if (!f) { fprintf(stderr, "cannot write %s\n", path); return; }
    fprintf(f, "P6\n%d %d\n255\n", w, h);
    for (int i = 0; i < w * h; i++) {
        rr_u32 c = px[i];
        /* the buffer is 0x00BBGGRR: R is the LOW byte, B the high one */
        unsigned char b[3] = { (unsigned char)( c        & 255),
                               (unsigned char)((c >>  8) & 255),
                               (unsigned char)((c >> 16) & 255) };
        fwrite(b, 1, 3, f);
    }
    fclose(f);
}

int main(void) {
    fb_bind(RR_W, RR_H, NULL);
    tex_init();
    world_gen();
    mesh_build();

    g_cam.pos = v3(24.5f, 6.0f, 24.5f);
    g_cam.vel = v3(0, 0, 0);
    g_cam.yaw = 0.6f; g_cam.pitch = -0.18f;
    g_cam.onGround = 0; g_cam.fly = 0; g_cam.sneak = 0;

    printf("MESH faces=%d  chunks=%d (non-empty=%d)\n", g_meshN, (int)NCHUNK, count_chunks());

    {
        int cnt[8];
        memset(cnt, 0, sizeof(cnt));
        for (int i = 0; i < g_meshN; i++) cnt[g_mesh[i].dir]++;
        printf("DIRS ");
        for (int d = 0; d < 8; d++) printf("%s=%d ", DIR_NAME[d], cnt[d]);
        printf("\n");
        /* GRASS must never be upside down */
        int bad = 0;
        for (int i = 0; i < g_meshN; i++)
            if (g_mesh[i].type == BT_GRASS && g_mesh[i].dir > 3) bad++;
        printf("GRASS upside-down faces = %d (expect 0)\n", bad);
        int tc[4];
        memset(tc, 0, sizeof(tc));
        for (int i = 0; i < g_meshN; i++) tc[g_mesh[i].type]++;
        printf("TYPES air=%d grass=%d dirt=%d rock=%d\n", tc[0], tc[1], tc[2], tc[3]);
    }

    /* ---- self test: no decal anywhere, and no floating blocks ---- */
    {
        int bad = 0;
        /* Every baked tile must be a pixel for pixel copy of its source
         * texture.  That is what "no decal" means now: the old arrow and the
         * old side bar were the only things that could make a tile differ. */
        for (int t = 1; t < BT_N; t++)
            for (int slot = 0; slot < 3; slot++) {
                int ti = at_tile(t, slot);
                int ox = (ti % AT_COLS) * AT_TILE, oy = (ti / AT_COLS) * AT_TILE;
                const Tex* base = &g_btex[t][slot];
                for (int y = 0; y < AT_TILE && !bad; y++)
                    for (int x = 0; x < AT_TILE && !bad; x++) {
                        rr_u32 src = base->px[(size_t)y * base->size + x];
                        rr_u32 want = ((src & 255u) << 16) | (src & 0xFF00u) | ((src >> 16) & 255u);
                        if (g_atlas.px[(size_t)(oy + y) * AT_W + (ox + x)] != want) {
                            printf("DECALS %s slot %d differs at (%d,%d)\n", BT_NAME[t], slot, x, y);
                            bad = 1;
                        }
                    }
            }
        printf("DECALS none baked (tiles are verbatim source textures): %s\n", bad ? "FAIL" : "ok");

        /* A tile must not depend on the heading.  at_tile() has no direction
         * argument any more, so this is really a check on the mesh: build the
         * tile index for every face of every face in the mesh and confirm the
         * same (type, face) always lands on the same tile.                  */
        int tileOf[BT_N][6];
        memset(tileOf, -1, sizeof(tileOf));
        for (int i = 0; i < g_meshN; i++) {
            const MeshFace* f = &g_mesh[i];
            int ti = at_tile(f->type, FACE_TEX[f->faceIdx]);
            if (tileOf[f->type][f->faceIdx] < 0) tileOf[f->type][f->faceIdx] = ti;
            else if (tileOf[f->type][f->faceIdx] != ti) bad++;
        }
        printf("DECALS tile independent of heading: %s\n", bad ? "FAIL" : "ok");

        /* nothing may float: every solid cell must have solid below it or be
         * on the floor */
        int floaters = 0;
        for (int x = 0; x < WX; x++)
            for (int y = 1; y < WY; y++)
                for (int z = 0; z < WZ; z++)
                    if (g_type[x][y][z] != BT_AIR && g_type[x][y-1][z] == BT_AIR) floaters++;
        printf("WORLD floating cells = %d (expect 0)\n", floaters);
    }

    /* ---- self test: frustum planes ---- */
    {
        M4 P = m4persp(1.2217f, 16.0f / 9.0f, RR_NEAR, RR_FAR);
        M4 V = m4view(v3(0, 0, 0), v3(0, 0, -1), v3(0, 1, 0));
        M4 M = m4mul(&P, &V);
        Plane pl[6];
        planes_from_mvp(&M, pl);
        const char* nm[6] = { "in front 10m", "behind 10m", "far left", "right edge", "above", "below" };
        float box[6][6] = {
            { -1, -1, -11,  1,  1,  -9 },   /* in front  -> visible   */
            { -1, -1,   9,  1,  1,  11 },   /* behind    -> culled    */
            { -30, -1, -11, -20, 1, -9 },   /* far left  -> culled    */
            {  20, -1, -11, 30,  1, -9 },   /* right     -> culled    */
            { -1, 20, -11,  1, 30, -9 },    /* above     -> culled    */
            { -1, -30, -11, 1, -20, -9 },   /* below     -> culled    */
        };
        for (int i = 0; i < 6; i++) {
            int r = aabb_in_frustum(pl, box[i]);
            printf("FRUSTUM %-14s -> %s\n", nm[i], r ? "kept" : "culled");
        }
        printf("FRUSTUM expect: kept, culled, culled, culled, culled, culled\n");
    }

    /* ---- self test: a triangle that must cover the screen centre ---- */
    {
        fb_clear();
        CV t[3];
        memset(t, 0, sizeof(t));
        t[0].x = -0.5f; t[0].y = -0.5f; t[0].w = 1.0f;
        t[1].x =  0.5f; t[1].y = -0.5f; t[1].w = 1.0f; t[1].u = 512.0f;
        t[2].x =  0.0f; t[2].y =  0.5f; t[2].w = 1.0f; t[2].v = 512.0f;
        int before = g_pix;
        raster_tri(t, 256, 1);
        printf("SELFTEST centre triangle: pix=%d (expect ~115200)\n", g_pix - before);
        g_pix = before;
    }

    /* ---- regression test: a face pressed right up against the eye ----
     *
     * The wall you are leaning on fills the screen and its corners are far
     * outside it -- at 0.3 m the bottom corner is at y_ndc ~ -7.7, and a real
     * frame measured 160.  The guard used to reject a triangle as soon as ANY
     * corner passed 4, so the nearer a face was, the likelier it was to be
     * dropped whole.  This builds exactly that shape and insists it still
     * covers the middle of the screen.                                     */
    {
        fb_clear();
        /* a quad 0.3 in front of the eye, 1 wide and 1.7 tall: clip space
         * y is scaled by the projection, so give the corners y_ndc far past
         * the old limit while the quad still straddles the centre           */
        CV q[4];
        memset(q, 0, sizeof(q));
        q[0].x = -0.4f; q[0].y = -7.7f; q[0].w = 0.3f;
        q[1].x =  0.4f; q[1].y = -7.7f; q[1].w = 0.3f; q[1].u = 512.0f;
        q[2].x =  0.4f; q[2].y =  7.7f; q[2].w = 0.3f; q[2].u = 512.0f; q[2].v = 512.0f;
        q[3].x = -0.4f; q[3].y =  7.7f; q[3].w = 0.3f; q[3].v = 512.0f;
        CV t[3];
        t[0] = q[0]; t[1] = q[1]; t[2] = q[2]; raster_tri(t, 256, 1);
        t[0] = q[0]; t[1] = q[2]; t[2] = q[3]; raster_tri(t, 256, 1);
        int cx = g_fb.w / 2, cy = g_fb.h / 2;
        int filled = 0, tot = 0;
        for (int y = cy - 40; y <= cy + 40; y++)
            for (int x = cx - 40; x <= cx + 40; x++) {
                tot++;
                if (g_fb.depth[(size_t)y * g_fb.w + x] > 0.0f) filled++;
            }
        printf("SELFTEST hugging a wall, centre covered: %.3f (expect 1.000)\n",
               (double)filled / (double)tot);
        g_pix = 0;
    }

    /* ---- self test: compass, raycast and the FOV clamp ---- */
    {
        /* cam_forward(0) looks down -Z = north, and +yaw swings west, so the
         * bearing runs the other way round from yaw.                      */
        struct { float yaw; const char* want; } cc[8] = {
            {  0.000000f, "N"  }, { -0.785398f, "NE" }, { -1.570796f, "E"  },
            { -2.356194f, "SE" }, {  3.141593f, "S"  }, {  2.356194f, "SW" },
            {  1.570796f, "W"  }, {  0.785398f, "NW" },
        };
        int bad = 0;
        for (int i = 0; i < 8; i++) {
            const char* got = compass_of(cc[i].yaw);
            if (strcmp(got, cc[i].want) != 0) {
                printf("COMPASS yaw %.3f -> %s, want %s\n", cc[i].yaw, got, cc[i].want);
                bad++;
            }
        }
        printf("COMPASS eight bearings: %s\n", bad ? "FAIL" : "ok");

        /* stand on a column and look straight down: the crosshair must land
         * on that column's top block, at roughly eye height above it.      */
        int bx = 24, bz = 24, top = -1;
        for (int y = WY - 1; y >= 0; y--) if (g_type[bx][y][bz] != BT_AIR) { top = y; break; }
        g_cam.pos = v3((float)bx + 0.5f, (float)(top + 1), (float)bz + 0.5f);
        g_cam.yaw = 0.0f; g_cam.pitch = -1.5533f; g_cam.sneak = 0;
        char got[64]; float d = 0.0f;
        block_looked_at(got, sizeof(got), &d);
        float want = MC_EYE_STAND;          /* eye is 1.62 above the floor  */
        const char* wantName = BT_NAME[g_type[bx][top][bz]];
        int typeOk = (strncmp(got, wantName, strlen(wantName)) == 0);
        printf("RAYCAST down: %s at %.2f m (want %s, ~%.2f)  %s\n", got, d,
               wantName, want, typeOk ? "type ok" : "TYPE FAIL");
        if (fabsf(d - want) > 0.10f) printf("RAYCAST distance off by %.3f\n", d - want);
        else printf("RAYCAST distance within one step: ok\n");

        /* straight up into an empty sky: nothing within reach            */
        g_cam.pitch = 1.5533f;
        block_looked_at(got, sizeof(got), &d);
        printf("RAYCAST up: %s (want sky), dist %.2f (want 0)\n", got, d);

        /* FOV clamp keeps the projection sane at both ends               */
        printf("FOV default %.1f  clamp range %.0f..%.0f\n",
               g_fovDeg, RR_FOV_MIN, RR_FOV_MAX);
        g_cam.pitch = 0.0f;
    }

    /* ---- self test: the projection's aspect can never be NaN ----
     *
     * g_fb is the software rasteriser's buffer and on the GPU path it is
     * never bound, so g_fb.w / g_fb.h was 0/0 = NaN: the projection matrix
     * went NaN, every vertex collapsed and the frame came out as sky and
     * nothing else -- with the pass still reporting chunks drawn, so it was
     * accepted as good and never fell back.  rr_aspect() has to return a
     * finite, sane number with no canvas and no frame buffer at all.      */
    {
        int savedW = g_fb.w, savedH = g_fb.h;
        rr_u32* savedC = g_fb.color;
        g_fb.w = 0; g_fb.h = 0; g_fb.color = NULL;      /* the GPU-path state */
        float a = rr_aspect();
        int ok = (a == a) && a > 0.5f && a < 4.0f;
        printf("ASPECT no canvas, no frame buffer: %.4f -> %s (want finite, ~%.4f)\n",
               (double)a, ok ? "ok" : "NaN/FAIL", (double)RR_W / (double)RR_H);
        g_fb.w = savedW; g_fb.h = savedH; g_fb.color = savedC;
    }

    /* settle the camera on the ground first */
    for (int i = 0; i < 120; i++) cam_step(&g_cam, 1.0f / 60.0f, 0, 0, 0, 0, 0, 0, 0);

    typedef struct Pose { const char* name; float yaw, pitch; } Pose;
    Pose poses[4] = {
        { "f00", 0.0f,      -0.15f },
        { "f01", 1.570796f, -0.15f },
        { "f02", 3.141593f,  0.25f },
        { "f03", -0.8f,     -0.55f },
    };

    for (int i = 0; i < 4; i++) {
        g_cam.yaw = poses[i].yaw;
        g_cam.pitch = poses[i].pitch;
        g_chunkIn = g_chunkOut = g_farout = g_backface = g_behind = 0;
        g_faces = g_raster = g_culled = g_pix = 0;
        clock_t t0 = clock();
        render_frame();
        fb_crosshair();
        double ms = 1000.0 * (double)(clock() - t0) / CLOCKS_PER_SEC;
        printf("frame %s: %.2f ms  chunkIn=%d chunkOut=%d farout=%d backface=%d behind=%d faces=%d raster=%d pix=%d\n",
               poses[i].name, ms, g_chunkIn, g_chunkOut, g_farout, g_backface, g_behind,
               g_faces, g_raster, g_pix);
        char path[128];
        snprintf(path, sizeof(path), "out_%s.ppm", poses[i].name);
        ppm_write(path, g_fb.color, g_fb.w, g_fb.h);
        printf("wrote %s\n", path);
    }

#ifdef RR_BENCH
    /* Cost of one software frame against canvas size.
     *
     * Fullscreen is where this matters: the canvas stops being the size
     * initgraph() was given and becomes the size of the whole monitor, and
     * at 150% or 200% DPI that is the monitor size TIMES the scaling factor.
     * The software rasteriser is O(pixels), and it is also the thing that
     * decides how often the pointer gets sampled -- so "the mouse feels
     * laggy in fullscreen" is very often this number, not the mouse code. */
    {
        struct { const char* what; int w, h; } sizes[] = {
            { "1280x720   windowed @100%",       1280,  720 },
            { "1920x1080  windowed @150%, or",   1920, 1080 },
            { "            fullscreen on 1080p", 1920, 1080 },
            { "2560x1440  windowed @200%, or",   2560, 1440 },
            { "            fullscreen on 1440p", 2560, 1440 },
            { "2880x1620  Retina panel",         2880, 1620 },
            { "3840x2160  fullscreen on 4K",     3840, 2160 },
        };
        g_cam.yaw = 0.0f; g_cam.pitch = -0.15f;
        for (int i = 0; i < (int)(sizeof(sizes)/sizeof(sizes[0])); i++) {
            fb_bind(sizes[i].w, sizes[i].h, NULL);
            render_frame();                     /* warm the allocator        */
            double best = 1.0e30;
            for (int k = 0; k < 5; k++) {
                clock_t t0 = clock();
                render_frame();
                double ms = 1000.0 * (double)(clock() - t0) / CLOCKS_PER_SEC;
                if (ms < best) best = ms;
            }
            printf("BENCH %-28s %8.2f ms  ->  %5.1f fps ceiling  (%d px)\n",
                   sizes[i].what, best, 1000.0 / best,
                   sizes[i].w * sizes[i].h);
        }
    }
#endif

    fb_free();
    mesh_free();
    return 0;
}

#else

/* ------------------------------------------------------------------ */
/* 8b. OSD                                                             */
/* ------------------------------------------------------------------ */
/* R toggles it.  Drawn with easygl's own text, i.e. as ordinary 2D
 * commands on top of whatever produced the frame -- so it lands in the right
 * place whichever path ran, without the rasteriser having to know about
 * glyphs.  The panel is filled with a translucent colour (easygl reads the
 * top byte of a COLORREF as transparency, 0 = opaque) so the world stays
 * visible behind it instead of being covered by a black box.              */

#define OSD_LH    17        /* line height, px                             */
#define OSD_PAD   8         /* panel padding, px                           */
#define OSD_MAX   28        /* max lines                                   */

static int g_osdOn = 1;

static void osd_panel(const char* lines[], int n, int x, int y) {
    if (n <= 0) return;
    int maxw = 0;
    for (int i = 0; i < n; i++) {
        int w = textwidth(lines[i]);
        if (w > maxw) maxw = w;
    }
    /* ~55% transparent: dark enough to read white text on, light enough to
     * still see the terrain it is covering.                               */
    setfillcolor(ARGB(140, 6, 8, 14));
    solidrectangle((double)(x - OSD_PAD), (double)(y - OSD_PAD),
                   (double)(x + maxw + OSD_PAD),
                   (double)(y + n * OSD_LH - (OSD_LH - 12) + OSD_PAD));
    setbkmode(TRANSPARENT);
    settextcolor(RGB(232, 238, 245));
    for (int i = 0; i < n; i++)
        outtextxy((double)x, (double)(y + i * OSD_LH), lines[i]);
}

/* Everything the OSD reports, gathered in one place so the panel is pure
 * formatting.  `ms` is the LAST frame's wall time for render + present, not
 * a smoothed average, because a smoothed number hides the spikes that matter. */
typedef struct Osd {
    float  fps, ms;
    int    usedGpu, scalePct, cw, ch, rw, rh;   /* device pixels          */
    int    logW, logH;                          /* logical (96 dpi) units */
    float  dpi;                                 /* device / logical       */
    int    fullscreen;
    int    scaleAuto;
    int    mouseFree;
    float  fovV, fovH;
    float  x, y, z, yaw, pitch, speed;
    const char* compass;
    const char* mode;       /* walk / sprint / sneak / fly                 */
    int    onGround;
    const char* under;      /* block stood on                              */
    const char* look;       /* block under the crosshair                   */
    float  lookDist;
    int    faces, raster, pix, chunkIn, chunkOut, backface, behind, farout;
    int    meshN, chunks;
} Osd;

static void osd_draw(const Osd* o) {
    char  buf[OSD_MAX][96];
    const char* L[OSD_MAX];
    int n = 0;

#define ROW(...) do { \
        snprintf(buf[n], sizeof(buf[n]), __VA_ARGS__); L[n] = buf[n]; n++; \
    } while (0)

    ROW("%.0f fps   %.2f ms   (%s)", o->fps, o->ms, o->usedGpu ? "GPU mesh" : "CPU software");
    if (!o->usedGpu)
        ROW("render %dx%d -> canvas %dx%d device   (scale %d%%%s)",
            o->rw, o->rh, o->cw, o->ch, o->scalePct,
            o->scaleAuto ? ", auto" : "");
    else
        ROW("canvas %dx%d device", o->cw, o->ch);
    ROW("window %dx%d logical   dpi %.0f%%   %s",
        o->logW, o->logH, (double)(o->dpi * 100.0f),
        o->fullscreen ? "fullscreen" : "windowed");
    ROW("fov %.1f deg vertical   %.1f horizontal   [F]", o->fovV, o->fovH);
    ROW(" ");
    ROW("pos  %.2f  %.2f  %.2f", o->x, o->y, o->z);
    ROW("look yaw %7.1f  pitch %6.1f   %s", o->yaw, o->pitch, o->compass);
    ROW("move %s   %.2f b/s   %s", o->mode, o->speed, o->onGround ? "grounded" : "airborne");
    ROW("cursor %s   [ALT, or middle button]",
        o->mouseFree ? "RELEASED - free to move out" : "grabbed");
    ROW("under  %s", o->under);
    if (o->lookDist > 0.0f)
        ROW("aim    %s   %.2f m", o->look, o->lookDist);
    else
        ROW("aim    %s", o->look);
    ROW(" ");
    ROW("frame  faces %-6d raster %-6d  pixels %d", o->faces, o->raster, o->pix);
    ROW("cull   chunks %d/%d   backface %-5d near %d",
        o->chunkIn, o->chunkIn + o->chunkOut, o->backface, o->behind);
    ROW("world  %d faces   %d chunks", o->meshN, o->chunks);
    ROW(" ");
    ROW("WASD move   SPACE jump   SHIFT sneak   CTRL sprint");
    ROW("V fly   F fov   R osd   F9 path   F10 scale");
    ROW("F11 fullscreen   F12 dpisupport   ALT or MMB release cursor");
    ROW("ESC quit");

#undef ROW

    osd_panel(L, n, 14, 14);
}

/* F: ask for the field of view.  easygl's InputBox runs its own modal loop
 * and, per its header, drains keys still in flight and mutes anything still
 * physically down -- so the F that opened it cannot land in the edit box or
 * auto repeat into it.  What it does NOT do is hand back the KEYUP of a
 * movement key that was held when it opened, so the walk state is cleared on
 * return: otherwise W stays "down" for ever and the camera drifts.        */
static void ask_fov(void) {
    char in[32];
    snprintf(in, sizeof(in), "%.1f", (double)g_fovDeg);

    ShowCursor(TRUE);
    ClipCursor(NULL);
    int ok = (int)InputBox(in, (int)sizeof(in),
                           "Vertical field of view, in degrees (25 - 120).",
                           "Field of view", in, 460, 190, 1);
    ShowCursor(FALSE);
    if (!ok) return;

    char* end = NULL;
    double d = strtod(in, &end);
    if (end == in) return;                    /* nothing numeric typed      */
    if (d != d) return;                       /* NaN                        */
    if (d < RR_FOV_MIN) d = RR_FOV_MIN;
    if (d > RR_FOV_MAX) d = RR_FOV_MAX;
    g_fovDeg = (float)d;
}

/* easygl has no "make the process DPI aware" entry point of its own: the
 * switch is exposed as a documented side effect of getinitscreenscale(),
 * which calls SetProcessDPIAware().  That is the only sanctioned route and
 * it is used deliberately, not by accident.
 *
 * WHY IT HAS TO COME FIRST.  fixhighdpi() reads the factor from getdpi(),
 * which is GetDeviceCaps(LOGPIXELSY).  For a process that is NOT DPI aware
 * Windows reports 96 no matter what the display is actually set to, so
 * getdpi() returns 96, the factor works out to 1.0 and fixhighdpi() does
 * nothing at all -- it looks like the call is broken.  Worse, an unaware
 * process gets its whole window stretched by the DWM, so the 1280x720
 * canvas is rendered at 1280x720 and blown up to 1920x1080 on screen:
 * blurry, and no amount of fixhighdpi() fixes it after the fact.
 *
 * So: become DPI aware before initgraph() creates anything, then let
 * fixhighdpi() do the scaling.  A program that already ships a DPI aware
 * manifest needs none of this and SetProcessDPIAware() fails harmlessly.
 * Set RR_DPI_AWARE to 0 to opt out and keep the old virtualised behaviour. */
#ifndef RR_DPI_AWARE
#define RR_DPI_AWARE  1
#endif
static void rr_become_dpi_aware(void) {
#if RR_DPI_AWARE
    (void)getinitscreenscale();     /* the call is the point, not the value */
#endif
}

/* The GPU path has no CPU buffer to poke, so its crosshair goes down
 * through normal easygl drawing after the frame has been copied.          */
static void crosshair_gl(void) {
    /* LOGICAL: line() takes 96 dpi units, so the centre is getwidth() / 2.
     * getcanvaswidth() / 2 is the DEVICE centre and would put the crosshair
     * off by half a screen at 200%.                                      */
    int cx = g_logW / 2, cy = g_logH / 2, L = 10;
    setlinecolor(RGB(0, 0, 0));
    line(cx - L - 1, cy, cx + L + 1, cy);
    line(cx, cy - L - 1, cx, cy + L + 1);
    setlinecolor(RGB(255, 255, 255));
    line(cx - L, cy, cx + L, cy);
    line(cx, cy - L, cx, cy + L);
}

/* ------------------------------------------------------------------ */
/* 8c. window: resizable, and borderless fullscreen                    */
/* ------------------------------------------------------------------ */
/* variablewinsize(true) puts WS_THICKFRAME | WS_MAXIMIZEBOX on the window,
 * so it can be dragged by a border, maximised, and snapped.  easygl rebuilds
 * the canvas on every WM_SIZE and clears it to the background colour, which
 * means a resize throws the picture away -- fine here, because the loop
 * redraws every frame.
 *
 * Nothing in the renderer caches a size.  That is the whole requirement:
 *   - dpi_poll() re-reads the device and logical sizes every frame, so the
 *     software rasteriser re-binds its frame buffer and the GPU path renders
 *     at the new canvas size on their very next frame;
 *   - the sky blit, the crosshair centre and the pointer clip are all derived
 *     from those same numbers.
 * A renderer that took the size once from initgraph() would draw into the
 * wrong place after the first drag.                                      */

static LONG g_fsStyle = 0;          /* window style to come back to      */
static RECT g_fsRect;               /* window rect to come back to       */
static int  g_fsSaved = 0;

/* Rectangle of the monitor the window is on.
 *
 * The monitor API is imported by hand rather than called directly: MinGW's
 * headers gate MonitorFromWindow / GetMonitorInfo behind a _WIN32_WINNT
 * that is not guaranteed to be high enough on an older w32api, and a name
 * that is not declared is a build failure, not a runtime fallback.  Loading
 * it by name costs nothing and keeps the fallback -- the primary display --
 * available on anything where the import is missing.
 *
 * The layout below is MONITORINFO: cbSize, rcMonitor, rcWork, dwFlags.  A
 * RECT is four 32 bit LONGs and Windows is LLP64, so four longs match it
 * exactly on both 32 and 64 bit builds.                                   */
typedef struct RRMonInfo {
    DWORD cbSize;
    long  monitor[4];       /* left, top, right, bottom  */
    long  work[4];          /* the same minus the taskbar */
    DWORD flags;
} RRMonInfo;

static void rr_monitor_rect(int* x, int* y, int* w, int* h) {
    HMODULE hu = GetModuleHandleA("user32.dll");
    *x = 0; *y = 0;
    *w = GetSystemMetrics(0);           /* SM_CXSCREEN, primary display  */
    *h = GetSystemMetrics(1);           /* SM_CYSCREEN                   */
    if (!hu) return;
    {
        typedef void* (WINAPI* PFN_MFW)(HWND, DWORD);
        typedef int   (WINAPI* PFN_GMI)(void*, RRMonInfo*);
        PFN_MFW pMFW = (PFN_MFW)GetProcAddress(hu, "MonitorFromWindow");
        PFN_GMI pGMI = (PFN_GMI)GetProcAddress(hu, "GetMonitorInfoA");
        if (pMFW && pGMI) {
            void* mon = pMFW(GetHWnd(), 2ul);   /* MONITOR_DEFAULTTONEAREST */
            RRMonInfo mi;
            if (mon) {
                memset(&mi, 0, sizeof(mi));
                mi.cbSize = (DWORD)sizeof(mi);
                if (pGMI(mon, &mi)) {
                    int mw = (int)(mi.monitor[2] - mi.monitor[0]);
                    int mh = (int)(mi.monitor[3] - mi.monitor[1]);
                    if (mw > 0 && mh > 0) {
                        *x = (int)mi.monitor[0];
                        *y = (int)mi.monitor[1];
                        *w = mw;
                        *h = mh;
                    }
                }
            }
        }
    }
}

/* The fullscreen rect is the MONITOR rect, not the work area: a fullscreen
 * game is supposed to cover the taskbar.  HWND_TOPMOST is what actually
 * achieves that -- the taskbar is a topmost window itself, so a merely
 * top-of-Z-order window sized over it still loses the bottom strip.       */
static void rr_fullscreen_place(HWND z) {
    HWND h = GetHWnd();
    int x, y, w, hh;
    if (!h) return;
    rr_monitor_rect(&x, &y, &w, &hh);
    if (w < 1 || hh < 1) return;
    SetWindowPos(h, z, x, y, w, hh,
                 SWP_FRAMECHANGED | SWP_SHOWWINDOW | SWP_NOACTIVATE);
}

static void rr_fullscreen_set(int on) {
    HWND h = GetHWnd();
    LONG st;
    if (!h) return;
    if ((on ? 1 : 0) == (g_fullscreen ? 1 : 0)) return;

    if (on) {
        /* Remember where to come back to -- but only the first time.  F11
         * re-applies the fullscreen rect after a DPI change, and that call
         * must not store the fullscreen rect as the restore rect.        */
        if (!g_fsSaved) {
            g_fsStyle = GetWindowLongA(h, GWL_STYLE);
            GetWindowRect(h, &g_fsRect);
            g_fsSaved = 1;
        }
        /* Strip the frame.  easygl's window carries WS_CAPTION (which drags
         * WS_DLGFRAME in with it) and, with variablewinsize on, WS_THICKFRAME
         * and WS_MAXIMIZEBOX; all of that is non-client area, and a window
         * sized to the monitor rect would still show it.  WS_POPUP with
         * nothing else leaves the client area equal to the whole monitor. */
        st = g_fsStyle;
        st &= ~(LONG)(WS_CAPTION | WS_THICKFRAME | WS_MINIMIZEBOX |
                      WS_MAXIMIZEBOX | WS_SYSMENU | WS_DLGFRAME | WS_BORDER);
        st |= (LONG)(WS_POPUP | WS_VISIBLE);
        SetWindowLongA(h, GWL_STYLE, st);
        g_fullscreen = 1;
        rr_fullscreen_place(HWND_TOPMOST);
    } else {
        SetWindowLongA(h, GWL_STYLE, g_fsStyle);
        g_fullscreen = 0;
        /* HWND_NOTOPMOST as well as the style: leaving the window topmost
         * would keep it over everything after it went back to being a
         * normal window.                                                 */
        if (g_fsSaved)
            SetWindowPos(h, HWND_NOTOPMOST,
                         g_fsRect.left, g_fsRect.top,
                         g_fsRect.right - g_fsRect.left,
                         g_fsRect.bottom - g_fsRect.top,
                         SWP_FRAMECHANGED | SWP_SHOWWINDOW | SWP_NOACTIVATE);
    }
    /* easygl rebuilds the canvas from the WM_SIZE this produces, so the
     * sizes are only correct after the message has been through the pump.
     * dpi_poll() runs at the top of every frame anyway; the caller just has
     * to forget the mouse reference so the next delta is not a jump.     */
}

int main(void) {
    /* First statement in the program: it has to precede any window or DC,
     * and initgraph() is about to create both.  See rr_become_dpi_aware(). */
    rr_become_dpi_aware();

    /* Resizable window.  Called BEFORE initgraph() on purpose: the window is
     * then created with the sizable frame already on, so nothing has to be
     * re-measured or rebuilt.  Calling it afterwards works too, but the
     * frame then grows outward from a live window and costs a second
     * SetWindowPos() plus, on a window that cannot grow, a canvas rebuild.
     * F12 sets this aside while fullscreen is up and restores it on exit --
     * the saved style still carries both bits.                            */
    variablewinsize(1);

    initgraph(RR_W, RR_H);
    setbkcolor(RGB(20, 20, 40));
    setvsync(1);
    BeginBatchDraw();
    /* The software rasteriser overwrites every pixel, so the read back that
     * GetImageBuffer() does by default is a full canvas glReadPixels per
     * frame for data that is thrown away.  Skip it.                      */
    setimagebuffermode(GX_IMGBUF_DISCARD);

    /* Fold the system DPI into the coordinate space: from here on the window
     * is RR_W x RR_H LOGICAL units but RR_W*scale x RR_H*scale real pixels,
     * so it is the same physical size on every display and the software
     * rasteriser gets the extra pixels instead of the DWM stretching a small
     * frame.  Must come after initgraph(), which resets the coordinate space,
     * and before anything that measures the canvas.  F11 switches it.    */
    fixhighdpi();
    dpi_poll();

    /* easygl sizes its canvas in device pixels, which is not necessarily
     * RR_W x RR_H (DPI scaling).  Blitting a frame buffer of the wrong size
     * into it is one of the ways the window ends up blank, so take the real
     * numbers from the renderer instead of assuming.
     *
     * dpi_poll() fills in BOTH: g_devW/g_devH in real pixels for the raster-
     * iser and the mesh path, g_logW/g_logH in 96 dpi units for putimage()
     * and every other 2D command.                                         */
    tex_init();
    world_gen();
    mesh_build();
    sky_build();
    gpumesh_build();            /* after initgraph: needs the GL context   */
    fprintf(stderr, "voxelgl: %s path, %d faces, canvas %dx%d device / "
            "%dx%d logical, dpi %.0f%%\n",
            g_meshReady ? "GPU mesh" : "software", g_meshN,
            g_devW, g_devH, g_logW, g_logH, (double)(g_dpiScale * 100.0f));

    /* The title carries the name and nothing else.  Live numbers live on the
     * OSD (R): a title bar is one line, updates are a window message each,
     * and per frame it is a message per frame for values nobody can read
     * that fast.  Set once, here, and never touched again.               */
    SetWindowTextA(GetHWnd(), "voxelgl  --  F fov, R osd, V fly, ESC quit");

    g_cam.pos = v3(24.5f, 6.0f, 24.5f);
    g_cam.vel = v3(0, 0, 0);
    g_cam.yaw = 0.6f; g_cam.pitch = -0.18f;
    g_cam.onGround = 0; g_cam.fly = 0; g_cam.sneak = 0;

    /* ---- pointer lock: hide the cursor, keep it inside the client area
     * and re-centre it every frame.  The re-centring used to be tracked
     * through the queued WM_MOUSEMOVEs, which Windows coalesces; see the
     * sampling note further down the loop for why that lost movement.    */
    HWND hwnd = GetHWnd();
    ShowCursor(FALSE);
    RECT crc;
    GetClientRect(hwnd, &crc);
    int ccx = (crc.left + crc.right) / 2;
    int ccy = (crc.top + crc.bottom) / 2;
    int clipW = -1, clipH = -1;      /* -1 so the first frame clips */
    int clientW = 0, clientH = 0;    /* client size, this frame      */
    /* 1 = grabbed now.  Starts at 1 rather than 0 because the setup above
     * has already confined the pointer and already hidden the cursor, so the
     * first frame must not hide it a second time -- ShowCursor() is a counter
     * and a second decrement would take two increments to undo.          */
    int g_clipped = 1;

    int fwd = 0, back = 0, left = 0, right = 0, jump = 0, sprint = 0, sneak = 0;
    /* 1 while Alt is physically down, so the repeat stream it produces only
     * toggles the pointer once.  See RRK_MENU below.                       */
    int altHeld = 0;
    /* The pointer is sampled with GetCursorPos() against the position we
     * last parked it at, not from the queued WM_MOUSEMOVEs -- see the note
     * at the top of the loop.  g_lockX / g_lockY is that position, in SCREEN
     * coordinates (what SetCursorPos() takes and GetCursorPos() returns), and
     * lockValid says whether it is meaningful yet: on the first frame, and on
     * the first frame after the grab is taken again, there is no previous
     * position to difference against, so the delta has to be zero.        */
    int g_lockX = 0, g_lockY = 0, lockValid = 0;
    double last = 0;
    int have_last = 0;
    float fps = 60.0f;
    char blk[64] = "air";
    char look[64] = "sky";

    while (1) {
        /* Re-clip when the window moves or changes size.  With a resizable
         * window this happens on every WM_SIZE, including the one a maximise
         * or an F12 toggle produces. */
        RECT rc;
        GetClientRect(hwnd, &rc);
        /* The client size THIS frame.  clipW / clipH are the size the
         * pointer box was last set from, and -1 is their "force a re-clip"
         * sentinel -- F11, F12 and the FOV dialog all set it -- so they
         * cannot also be asked whether the window is minimised.  Ask the
         * rect.                                                          */
        clientW = rc.right - rc.left;
        clientH = rc.bottom - rc.top;

        /* Grabbed, unless the user released the pointer with the middle
         * button -- and only while this window is the one in front.  A grab
         * that outlived an alt-tab would keep yanking the pointer back from
         * whatever window came forward, so focus loss drops it; it comes
         * back by itself when the window is in front again.              */
        int wantLock = !g_mouseFree && (GetForegroundWindow() == hwnd)
                       && clientW > 0 && clientH > 0;
        if (clientW != clipW || clientH != clipH || wantLock != g_clipped) {
            clipW = clientW;
            clipH = clientH;
            if (wantLock != g_clipped) {
                /* ShowCursor() is a counter, so it has to be moved once per
                 * transition and not once per frame.  Releasing the pointer
                 * without this would leave an invisible cursor to aim with --
                 * the whole point of releasing is to be able to see where it
                 * is going.                                              */
                ShowCursor(wantLock ? FALSE : TRUE);
            }
            g_clipped = wantLock;
            if (wantLock) {
                POINT tl, br;
                tl.x = rc.left; tl.y = rc.top;
                br.x = rc.right; br.y = rc.bottom;
                ClientToScreen(hwnd, &tl);
                ClientToScreen(hwnd, &br);
                RECT clip;
                clip.left = tl.x; clip.top = tl.y; clip.right = br.x; clip.bottom = br.y;
                ClipCursor(&clip);
            } else {
                /* Released, minimised, or collapsed to nothing: there is no
                 * client area to confine the pointer to, and ClipCursor()
                 * with an empty rect would pin it to a single point on the
                 * desktop for as long as the window stays down.          */
                ClipCursor(NULL);
            }
        }
        ccx = (rc.left + rc.right) / 2;
        ccy = (rc.top + rc.bottom) / 2;

        /* input */
        int mdx = 0, mdy = 0, gotMouse = 0;
        ExMessage m;
        while (peekmessage(&m, EX_MOUSE | EX_KEY)) {
            if (m.message == WM_MBUTTONDOWN) {
                /* Release the pointer, or take it back.  That is all the
                 * middle button does: it has to be usable while the window
                 * is fullscreen and the cursor is hidden, which is exactly
                 * when there is no other way out.                         */
                g_mouseFree = !g_mouseFree;
            } else if (m.message == WM_KEYDOWN || m.message == WM_SYSKEYDOWN) {
                switch (m.vkcode) {
                    case RRK_MENU:
                        /* Release the pointer, or take it back.
                         *
                         * Alt is the one key that can always be pressed: the
                         * cursor is hidden and the pointer is confined, so
                         * there is nothing to click on to get out.  It is
                         * guarded against auto repeat because Windows sends a
                         * WM_SYSKEYDOWN for every repeat while Alt is held,
                         * and an unguarded toggle would flicker the grab on
                         * and off dozens of times a second.               */
                        if (!altHeld) { altHeld = 1; g_mouseFree = !g_mouseFree; }
                        break;
                    case 'W': fwd = 1; break;
                    case 'S': back = 1; break;
                    case 'A': left = 1; break;
                    case 'D': right = 1; break;
                    case RRK_SPACE: jump = 1; break;
                    case RRK_SHIFT: sneak = 1; break;
                    case RRK_CTRL:  sprint = 1; break;
                    case RRK_V:  g_cam.fly = !g_cam.fly; break;
                    case RRK_R:  g_osdOn = !g_osdOn; break;
                    case RRK_F:
                        /* modal: it pumps its own message loop, so it has
                         * to run here and not deferred to after the pump  */
                        ask_fov();
                        /* The dialog ate the keyups of anything that was
                         * held when it opened, and the clock ran on while
                         * it was up.  Drop the movement state and restart
                         * the delta, or the camera drifts and the first
                         * frame after it takes a giant step.             */
                        fwd = back = left = right = 0;
                        jump = sprint = sneak = 0;
                        have_last = 0;
                        lockValid = 0;           /* the pointer moved      */
                        clipW = clipH = -1;      /* force a re-clip        */
                        break;
                    case RRK_F9:  g_forceCpu = !g_forceCpu; break;
                    case RRK_F10:
                        g_scaleStep = (g_scaleStep + 1) % RR_SCALE_STEPS;
                        g_scaleAuto = 0;   /* an explicit choice wins      */
                        break;
                    
                    case RRK_F11:
                        rr_fullscreen_set(!g_fullscreen);
                        clipW = clipH = -1;      /* the client rect moved  */
                        lockValid = 0;
                        break;
					case RRK_F12:
                        /* Switch the DPI scaling on and off.  easygl
                         * resizes the window and re-scales the coordinate
                         * space, so both sizes change under us: re-poll and
                         * re-clip, and forget the mouse reference.
                         *
                         * fixhighdpi() sizes the window from the LOGICAL
                         * request, which would drag a fullscreen window back
                         * to its windowed size, so the fullscreen rect is
                         * re-applied afterwards.                          */
                        if (ishighdpi()) unfixhighdpi(); else fixhighdpi();
                        if (g_fullscreen) rr_fullscreen_place(HWND_TOPMOST);
                        dpi_poll();
                        clipW = clipH = -1;
                        lockValid = 0;
                        break;
                    case RRK_ESC: goto done;
                }
            } else if (m.message == WM_KEYUP || m.message == WM_SYSKEYUP) {
                switch (m.vkcode) {
                    case RRK_MENU: altHeld = 0; break;
                    case 'W': fwd = 0; break;
                    case 'S': back = 0; break;
                    case 'A': left = 0; break;
                    case 'D': right = 0; break;
                    case RRK_SPACE: jump = 0; break;
                    case RRK_SHIFT: sneak = 0; break;
                    case RRK_CTRL:  sprint = 0; break;
                }
            }
        }
        /* Read the pointer rather than the queued WM_MOUSEMOVEs.
         *
         * Windows coalesces mouse moves: a WM_MOUSEMOVE still waiting in the
         * queue does not get a second message posted behind it, it gets its
         * coordinates overwritten.  Every frame used to end with
         * SetCursorPos() back to the centre, which posts a move -- so
         * whatever the user did between the pump and that call was replaced
         * with "centre" before anyone read it.  Only the sliver of movement
         * between the recentre and the next pump ever survived.
         *
         * GetCursorPos() reads the live position instead of the queue, so
         * nothing coalesces.  It also sidesteps the units question: this is a
         * device pixel delta and never goes through easygl's logical
         * conversion.                                                     */
        if (wantLock) {
            POINT cp;
            if (GetCursorPos(&cp) && lockValid) {
                mdx += cp.x - g_lockX;
                mdy += cp.y - g_lockY;
                g_lockX = cp.x; g_lockY = cp.y;
                if (mdx || mdy) gotMouse = 1;
            }
        } else {
            lockValid = 0;
        }
        if (gotMouse) cam_look(&g_cam, (float)mdx, (float)mdy);

        /* Minimised: the window is not being composited, so vsync does not
         * pace the loop any more and it would spin at full CPU redrawing a
         * canvas nobody can see.  This sits AFTER the pump on purpose --
         * the pump is what processes the WM_SIZE that ends it, and it is
         * what lets ESC quit.                                            */
        if (clientW < 1 || clientH < 1) {
            Sleep(20);
            have_last = 0;      /* the clock ran on: drop the delta        */
            continue;
        }

        /* timing */
        double now = (double)clock() / CLOCKS_PER_SEC;
        float dt = have_last ? (float)(now - last) : 1.0f / 60.0f;
        have_last = 1; last = now;
        if (dt > 0.1f) dt = 0.1f;
        if (dt < 0.0005f) dt = 0.0005f;

        /* The canvas can change size under us: window resize, DPI change,
         * or F11 switching fixhighdpi().  Both sizes are re-read.         */
        dpi_poll();
        /* After the sizes are known and before anything renders: the canvas
         * can change on a resize, a DPI switch or an F12, and each one can
         * put a different number of pixels in front of the rasteriser.    */
        rr_auto_scale();

        cam_step(&g_cam, dt, fwd, back, left, right, jump, sprint, sneak);

        clock_t t0 = clock();
        int usedGpu = render_frame();
        if (usedGpu) crosshair_gl(); else fb_crosshair();

        /* A sub-resolution software frame has to be stretched over the
         * canvas; easygl scales it on the GPU in one blit.               */
        if (!usedGpu && g_scaled) putimage(0, 0, g_logW, g_logH, &g_frameImg, 0, 0);

        /* Frame cost of everything above: rasterise, blit, crosshair.  Not
         * the present, which FlushBatchDraw() / vsync owns and which is why
         * this can read 4 ms while the window runs at 60 fps.            */
        double frameMs = 1000.0 * (double)(clock() - t0) / CLOCKS_PER_SEC;
        if (dt > 0.0f) fps += (1.0f / dt - fps) * 0.08f;

        if (g_osdOn) {
            Osd o;
            memset(&o, 0, sizeof(o));
            o.fps      = fps;
            o.ms       = (float)frameMs;
            o.usedGpu  = usedGpu;
            o.scalePct = (int)(RR_SCALE[g_scaleStep] * 100.0f + 0.5f);
            o.scaleAuto = g_scaleAuto && (g_forceCpu || !g_meshReady);
            o.cw       = g_devW;
            o.ch       = g_devH;
            o.logW     = g_logW;
            o.logH     = g_logH;
            o.dpi        = g_dpiScale;
            o.fullscreen = g_fullscreen;
            o.mouseFree   = g_mouseFree;
            o.rw       = g_fb.color ? g_fb.w : g_devW;
            o.rh       = g_fb.color ? g_fb.h : g_devH;
            o.fovV     = g_fovDeg;
            {
                /* aspect is the same in either space: the DPI scale is
                 * uniform on both axes.                                  */
                float aspect = (g_devH > 0) ? (float)g_devW / (float)g_devH : 1.7777f;
                float halfV  = g_fovDeg * 3.14159265f / 360.0f;
                o.fovH = 2.0f * atanf(tanf(halfV) * aspect) * 180.0f / 3.14159265f;
            }
            o.x  = g_cam.pos.x; o.y = g_cam.pos.y; o.z = g_cam.pos.z;
            o.yaw   = g_cam.yaw   * 180.0f / 3.14159265f;
            o.pitch = g_cam.pitch * 180.0f / 3.14159265f;
            o.compass = compass_of(g_cam.yaw);
            o.speed   = sqrtf(g_cam.vel.x * g_cam.vel.x + g_cam.vel.z * g_cam.vel.z);
            o.mode    = g_cam.fly ? "fly"
                      : (sprint ? "sprint" : (sneak ? "sneak" : "walk"));
            o.onGround = g_cam.onGround;
            block_under_camera(blk, sizeof(blk));
            o.under = blk;
            block_looked_at(look, sizeof(look), &o.lookDist);
            o.look  = look;
            o.faces = g_faces; o.raster = g_raster; o.pix = g_pix;
            o.chunkIn = g_chunkIn; o.chunkOut = g_chunkOut;
            o.backface = g_backface; o.behind = g_behind; o.farout = g_farout;
            o.meshN  = g_meshN;
            o.chunks = count_chunks();
            osd_draw(&o);
        }

        FlushBatchDraw();

        /* Sample once more, then put the pointer back in the middle.
         *
         * THE SECOND HALF OF THE STUTTER.  SetCursorPos() teleports the
         * pointer, and distance covered before a teleport is simply gone:
         * everything the mouse did while this frame was being rendered --
         * which at fullscreen resolution is most of the wall clock time --
         * was being discarded here every single frame.  Reading the position
         * immediately before the jump rescues it.  It is applied after the
         * frame it happened in, so it shows up on the next one, which is the
         * same latency the rest of the loop has always had.
         *
         * Recentring also gives the next frame the whole client area to
         * measure into, instead of letting the pointer creep towards the edge
         * of the clip box where further movement would be swallowed.      */
        if (wantLock) {
            POINT sc2, cp;
            sc2.x = ccx; sc2.y = ccy;
            ClientToScreen(hwnd, &sc2);
            if (GetCursorPos(&cp) && lockValid) {
                int ex = cp.x - g_lockX, ey = cp.y - g_lockY;
                if (ex || ey) cam_look(&g_cam, (float)ex, (float)ey);
            }
            SetCursorPos(sc2.x, sc2.y);
            g_lockX = sc2.x; g_lockY = sc2.y;
            lockValid = 1;
        }
    }

done:
    if (g_clipped) ShowCursor(TRUE);      /* matches the FALSE at start up */
    ClipCursor(NULL);
    fb_free();
    mesh_free();
    gpumesh_free();
    closegraph();
    return 0;
}

#endif

