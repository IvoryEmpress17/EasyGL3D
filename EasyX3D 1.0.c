/*
 * voxelgl.cpp -- a small voxel renderer built on top of easygl.h
 *
 * Written from scratch.  Design rules that the previous renderer broke, and
 * which are honoured here:
 *
 *   1. ONE coordinate convention, stated once, used everywhere.
 *        world   : right handed, +Y up, -Z is north (Minecraft convention)
 *        camera  : looks down its own -Z, yaw around +Y, pitch positive = up
 *        clip    : w_clip = -z_view, so w_clip > 0 means "in front of camera"
 *        ndc     : x,y in [-1,1], z in [-1,1]  (OpenGL convention)
 *        screen  : x right, y DOWN, origin top left
 *   2. Screen y is flipped exactly once, in the viewport transform.  Nothing
 *      else in the pipeline ever flips anything.
 *   3. The depth buffer is cleared every frame.  (It is a frame buffer, not a
 *      persistent cache -- that was the bug that made the old renderer show
 *      stale geometry.)
 *   4. Nothing is written when the frame buffer does not exist yet.
 *   5. The world is static, so the visible-face list is built ONCE.  A frame
 *      then only transforms and rasterises; it never walks the voxel grid.
 *
 * Block data
 *   Every cell carries a type (AIR / GRASS / DIRT / ROCK) and a direction
 *   0..7 = east, south, west, north, upside-down-east, ... upside-down-north.
 *   Direction is shown with an arrow on the top face (bright = upright,
 *   dark = upside down) and a bar on the face the block looks at (top half
 *   when upright, bottom half when upside down).
 *
 * Controls:  WASD move, SPACE jump, SHIFT sneak, CTRL sprint, F fly, ESC quit
 *            The pointer is locked to the window and a crosshair is drawn.
 *
 * Build (real, Windows + MinGW):
 *     g++ -std=c++11 -O2 voxelgl.cpp -o voxelgl.exe -lopengl32 -luser32 -lgdi32
 *
 * Build (headless verification, any OS -- dumps frames to .ppm):
 *     g++ -std=c++11 -O2 -DRR_HEADLESS voxelgl.cpp -o vr
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdint.h>
#include <time.h>
#include <stdbool.h>

#ifndef RR_HEADLESS
#  include "easygl.h"
#endif

typedef uint32_t rr_u32;
typedef int32_t  rr_i32;
typedef float    rr_f32;

/* ------------------------------------------------------------------ */
/* 0. tunables                                                         */
/* ------------------------------------------------------------------ */

#define RR_W              1280
#define RR_H              720

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

/* Key codes.  easygl.h only defines VK_ESCAPE / VK_RETURN itself and relies
 * on windows.h for the rest, so define what we need here instead.          */
#define RRK_SPACE   0x20
#define RRK_SHIFT   0x10
#define RRK_CTRL    0x11
#define RRK_ESC     0x1B
#define RRK_F9      0x78

/* ------------------------------------------------------------------ */
/* 1. tiny math                                                        */
/* ------------------------------------------------------------------ */

typedef struct V3 { float x, y, z; } V3;

static inline V3 v3(float x, float y, float z) { V3 r; r.x = x; r.y = y; r.z = z; return r; }
static inline V3 vadd(V3 a, V3 b) { return v3(a.x + b.x, a.y + b.y, a.z + b.z); }
static inline V3 vsub(V3 a, V3 b) { return v3(a.x - b.x, a.y - b.y, a.z - b.z); }
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
    M4 r; memset(&r, 0, sizeof(r));
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

    /* ROW MAJOR again: row r holds basis vector r, translation in column 3. */
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
    M4 r; memset(&r, 0, sizeof(r));
    /* NOTE: this file stores matrices ROW MAJOR (m[r*4+c]).  In that layout
     * the "-1" that feeds w_clip belongs in row 3 / column 2, i.e. m[14].
     * Getting this backwards makes w_clip negative for everything in front
     * of the camera, and the near clip then discards the entire scene.      */
    r.m[0] = t / aspect;
    r.m[5] = t;
    r.m[10] = (f + n) / (n - f);
    r.m[11] = (2.0f * f * n) / (n - f);
    r.m[14] = -1.0f;
    return r;
}

/* ------------------------------------------------------------------ */
/* 1b. debug counters                                                  */
/* ------------------------------------------------------------------ */

static int g_faces = 0, g_raster = 0, g_culled = 0, g_clipped = 0, g_pix = 0, g_zfail = 0;
static int g_bboxbad = 0, g_outside = 0, g_iwbad = 0, g_zrange = 0;
static int g_zlow = 0, g_zhigh = 0; static float g_zmin = 1e30f, g_zmax = -1e30f;
static float g_wmin = 1e30f;
static int g_chunkIn = 0, g_chunkOut = 0, g_backface = 0, g_farout = 0;

/* ------------------------------------------------------------------ */
/* 2. frame buffer                                                     */
/* ------------------------------------------------------------------ */

typedef struct Frame {
    int w, h;
    rr_u32* color;      /* 0x00RRGGBB internally                          */
    float*  depth;      /* ndc z, cleared to +1 (far) every frame          */
} Frame;

static Frame g_fb = { 0, 0, NULL, NULL };

static void fb_free(void);

static float g_fogInv = 0.0f;     /* 1/(fogFar-fogNear), set in fb_init */

static void fb_init(int w, int h) {
    fb_free();
    if (w < 1) w = 1;
    if (h < 1) h = 1;
    g_fb.w = w; g_fb.h = h;
    g_fogInv = 1.0f / (RR_FOG_FAR - RR_FOG_NEAR);
    g_fb.color = (rr_u32*)malloc((size_t)w * h * sizeof(rr_u32));
    g_fb.depth = (float*)malloc((size_t)w * h * sizeof(float));
    if (!g_fb.color || !g_fb.depth) { fprintf(stderr, "out of memory\n"); exit(1); }
}

static void fb_free(void) {
    free(g_fb.color); free(g_fb.depth);
    g_fb.color = NULL; g_fb.depth = NULL; g_fb.w = g_fb.h = 0;
}

/* Plain sky gradient.  No sun: it was a hard coded disc that had nothing to
 * do with the light direction and looked wrong from most angles.          */
static void fb_sky(void) {
    if (!g_fb.color) return;
    for (int y = 0; y < g_fb.h; y++) {
        float t = (float)y / (float)(g_fb.h - 1);
        int r = (int)(46 + 88 * t);
        int g = (int)(96 + 104 * t);
        int b = (int)(168 + 62 * t);
        rr_u32 c = ((rr_u32)r << 16) | ((rr_u32)g << 8) | (rr_u32)b;
        rr_u32* row = g_fb.color + (size_t)y * g_fb.w;
        for (int x = 0; x < g_fb.w; x++) row[x] = c;
    }
}

static void fb_clear_depth(void) {
    if (!g_fb.depth) return;
    /* The buffer holds 1/w -- not z, not distance -- so nearer is LARGER and
     * "nothing here yet" is the smallest possible value: 0.  Filling it with
     * 1.0 made every fragment fail the (iw > depth) test -- a face 10m away
     * has iw = 0.1 -- which erased the whole world and left only the sky. */
    for (int i = 0; i < g_fb.w * g_fb.h; i++) g_fb.depth[i] = 0.0f;
}

static inline void ch_put(int x, int y, rr_u32 c) {
    if (x < 0 || y < 0 || x >= g_fb.w || y >= g_fb.h) return;
    g_fb.color[(size_t)y * g_fb.w + x] = c;
}

/* Crosshair, drawn into our own buffer so it needs no library support and
 * cannot be lost by a later blit.                                        */
static void fb_crosshair(void) {
    if (!g_fb.color) return;
    int cx = g_fb.w / 2, cy = g_fb.h / 2;
    const int L = 10, G = 3;
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

/* direction -> world axis.  0 east(+X) 1 south(+Z) 2 west(-X) 3 north(-Z),
 * 4..7 are the same headings with the block turned upside down.           */
static const int DIR_FACE[4]  = { 3, 5, 2, 4 };
static const char* DIR_NAME[8] = { "east", "south", "west", "north",
                                   "upside-down-east", "upside-down-south",
                                   "upside-down-west", "upside-down-north" };

typedef struct Tex { int size, w, h; rr_u32* px; } Tex;
static Tex g_btex[BT_N][3];        /* [type][0=top 1=side 2=bottom]        */

static unsigned hash2(int x, int y, unsigned seed) {
    unsigned h = (unsigned)x * 374761393u + (unsigned)y * 668265263u + seed * 2246822519u;
    h = (h ^ (h >> 13)) * 1274126177u;
    return h ^ (h >> 16);
}

/* Minecraft-ish procedural block texture: flat base colour plus per pixel
 * grit, optionally with a grass fringe along the u=1 edge (u is the
 * vertical axis on every side face -- see the face table below).          */
static void tex_block(Tex* t, int size, int r, int g, int b, int amp, int grass) {
    t->size = size; t->w = size; t->h = size;
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
            if (grass) {
                /* u == x here; the fringe sits at the top of the side face */
                float fu = (float)x / (float)(size - 1);
                if (fu > 0.80f) {
                    float k = (fu - 0.80f) / 0.20f;
                    /* ragged edge so the fringe is not a straight line */
                    k *= (0.55f + 0.45f * ((float)(h1 % 1000u) / 1000.0f));
                    if (k > 1.0f) k = 1.0f;
                    int gr = 106, gg = 170, gb = 60;
                    R = (int)(R + (gr - R) * k);
                    G = (int)(G + (gg - G) * k);
                    B = (int)(B + (gb - B) * k);
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
    /* w/h default to size (square block textures) */
    tex_block(&g_btex[BT_GRASS][0], 64, 106, 170,  60, 14, 0);   /* grass top    */
    tex_block(&g_btex[BT_GRASS][1], 64, 134,  96,  67, 14, 1);   /* grass side   */
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
/* Every (type, face, heading) combination is pre-rendered into one tile.
 * The decals -- the heading arrow on top, the bar on the facing side -- stop
 * being per pixel work and become something the rasteriser looks up like any
 * other texel.  It also means the software path and the GPU path sample the
 * same pixels and cannot drift apart.                                   */
#define AT_TILE   64
#define AT_COLS   16
#define AT_ROWS   16
#define AT_NTILE  (AT_COLS * AT_ROWS)
#define AT_W      (AT_TILE * AT_COLS)
#define AT_H      (AT_TILE * AT_ROWS)
static Tex g_atlas;                 /* CPU copy, 0x00RRGGBB                  */

static inline int at_tile(int type, int fi, int dir) {
    return ((type - 1) * 6 + fi) * 8 + dir;      /* 3 * 6 * 8 = 144 tiles   */
}


static const char* BT_NAME[4] = { "air", "GRASS", "DIRT", "ROCK" };

static inline rr_u32 tex_sample(const Tex* t, float u, float v) {
    int s = t->size;
    int x = (int)(u * s) & (s - 1);
    int y = (int)(v * s) & (s - 1);
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    return t->px[(size_t)y * s + x];
}

/* Arrow stencil in unit square, pointing towards +v.  Used for the top face
 * so a block's heading can be read from above.                            */
static inline int arrow_mask(float u, float v) {
    float a = u - 0.5f, b = v - 0.5f;
    if (b > -0.30f && b < 0.10f && fabsf(a) < 0.075f) return 1;      /* shaft */
    if (b >= 0.10f && b <= 0.38f) {
        float k = (0.38f - b) / 0.28f;                               /* head  */
        if (fabsf(a) < 0.24f * k) return 1;
    }
    return 0;
}

/* ------------------------------------------------------------------ */
/* 3b. rasteriser                                                      */
/* ------------------------------------------------------------------ */

/* A vertex that has already been through the model/view/projection matrix.
 * `w` is the clip w; it is kept so attributes can be interpolated with the
 * reciprocal (perspective correct) rule.                                  */
typedef struct CVtx { float x, y, z, w; float u, v; float du, dv; float light; } CVtx;

/* Sutherland-Hodgman against the single plane  w >= eps.  In clip space the
 * near plane is exactly w = eps, so this is the correct place to clip; doing
 * it before the divide is what avoids the "geometry vanishes" class of bug. */
static int clip_near(const CVtx* in, int n, CVtx* out, float eps) {
    int m = 0;
    for (int i = 0; i < n; i++) {
        const CVtx* A = &in[i];
        const CVtx* B = &in[(i + 1) % n];
        float da = A->w - eps, db = B->w - eps;
        bool ina = da >= 0, inb = db >= 0;
        if (ina) out[m++] = *A;
        if (ina != inb) {
            float t = da / (da - db);
            CVtx C;
            C.x = A->x + (B->x - A->x) * t;
            C.y = A->y + (B->y - A->y) * t;
            C.z = A->z + (B->z - A->z) * t;
            C.w = A->w + (B->w - A->w) * t;
            C.u = A->u + (B->u - A->u) * t;
            C.v = A->v + (B->v - A->v) * t;
            C.du = A->du + (B->du - A->du) * t;
            C.dv = A->dv + (B->dv - A->dv) * t;
            C.light = A->light + (B->light - A->light) * t;
            out[m++] = C;
        }
    }
    return m;   /* 0, 3 or 4 */
}

/* Distance used for fog.  Recovered from ndc z with the standard inverse of
 * the projection instead of assuming a linear relationship.                */
static float g_fogA, g_fogB;
static inline float zndc_to_dist(float z) {
    /* z_ndc = A + B / d   =>   d = B / (z - A)                            */
    return g_fogB / (z - g_fogA);
}

/* decal: 0 none, 1 arrow on top face, 2 bar on the "looking" side face.
 * mlo/mhi bound the bar vertically; dr/dg/db is the decal colour.         */
static void raster_tri(CVtx a, CVtx b, CVtx c, float light) {
    g_raster++;
    if (!g_fb.color) return;

    /* --- to ndc, keeping 1/w for perspective correct interpolation --- */
    CVtx src[3] = { a, b, c };
    CVtx v[3];
    for (int i = 0; i < 3; i++) {
        float iw = 1.0f / src[i].w;
        v[i].x = src[i].x * iw;
        v[i].y = src[i].y * iw;
        v[i].w = iw;                    /* 1/w from here on                */
        v[i].u = src[i].u * iw;
        v[i].v = src[i].v * iw;
    }

    /* --- to screen.  y flip happens here and nowhere else. --- */
    float sx[3], sy[3];
    for (int i = 0; i < 3; i++) {
        sx[i] = (v[i].x * 0.5f + 0.5f) * g_fb.w;
        sy[i] = (0.5f - v[i].y * 0.5f) * g_fb.h;
        if (!(fabsf(v[i].x) < 4.0f) || !(fabsf(v[i].y) < 4.0f)) return;
    }

    float area = (sx[1]-sx[0])*(sy[2]-sy[0]) - (sx[2]-sx[0])*(sy[1]-sy[0]);
    if (area > 0) {
        float tx = sx[1], ty = sy[1]; sx[1] = sx[2]; sy[1] = sy[2]; sx[2] = tx; sy[2] = ty;
        CVtx tv = v[1]; v[1] = v[2]; v[2] = tv;
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
    if (minx > maxx || miny > maxy) { g_bboxbad++; return; }

    float e0x = sx[1]-sx[0], e0y = sy[1]-sy[0];
    float e1x = sx[2]-sx[1], e1y = sy[2]-sy[1];
    float e2x = sx[0]-sx[2], e2y = sy[0]-sy[2];

    /* Divide by -area ONCE, here.  The three edge functions then sum to 1
     * and are the barycentric weights outright, which takes a division out
     * of every pixel.  They used to be normalised per pixel.              */
    float inv = 1.0f / (-area);
    float xs = (float)minx + 0.5f, ys = (float)miny + 0.5f;
    float b0 = ((xs-sx[0])*e0y - (ys-sy[0])*e0x) * inv;
    float b1 = ((xs-sx[1])*e1y - (ys-sy[1])*e1x) * inv;
    float b2 = ((xs-sx[2])*e2y - (ys-sy[2])*e2x) * inv;
    float g0x = e0y*inv, g0y = -e0x*inv;      /* d/dx and d/dy of each     */
    float g1x = e1y*inv, g1y = -e1x*inv;
    float g2x = e2y*inv, g2y = -e2x*inv;

    const float fogN = RR_FOG_NEAR, fogI = g_fogInv;
    const float fR = 150.0f, fG = 185.0f, fB = 225.0f;

    for (int py = miny; py <= maxy; py++) {
        float wy = (float)py + 0.5f - ys;
        float r0 = b0 + g0y * wy, r1 = b1 + g1y * wy, r2 = b2 + g2y * wy;

        /* Exact row span.  A plain bounding box visits every pixel in the
         * rectangle, and measurement said 1.74 M of the 2.68 M visits per
         * frame landed outside the triangle -- two thirds of the work went
         * on rejecting pixels.  Solving each edge for the x where it crosses
         * zero gives the true extent instead.                              */
        float lo = (float)minx, hi = (float)maxx, t;
        int empty = 0;
        if      (g0x >  1e-12f) { t = xs - r0/g0x - 0.5f; if (t > lo) lo = t; }
        else if (g0x < -1e-12f) { t = xs - r0/g0x - 0.5f; if (t < hi) hi = t; }
        else if (r0 < 0.0f) empty = 1;
        if      (g1x >  1e-12f) { t = xs - r1/g1x - 0.5f; if (t > lo) lo = t; }
        else if (g1x < -1e-12f) { t = xs - r1/g1x - 0.5f; if (t < hi) hi = t; }
        else if (r1 < 0.0f) empty = 1;
        if      (g2x >  1e-12f) { t = xs - r2/g2x - 0.5f; if (t > lo) lo = t; }
        else if (g2x < -1e-12f) { t = xs - r2/g2x - 0.5f; if (t < hi) hi = t; }
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

        for (int px = ix0; px <= ix1; px++) {
            /* iw is 1/w, and w IS the view space distance, so 1/w is large
             * up close and small far away: the depth test is a plain greater
             * than and no z interpolation is needed at all.               */
            float iw = w1*v[0].w + w2*v[1].w + w0*v[2].w;
            if (iw > drow[px]) {
                float riw = 1.0f / iw;
                float u  = (w1*v[0].u + w2*v[1].u + w0*v[2].u) * riw;
                float vv = (w1*v[0].v + w2*v[1].v + w0*v[2].v) * riw;

                rr_u32 tc = tex_sample(&g_atlas, u, vv);
                float tr = (float)((tc >> 16) & 255) * light;
                float tg = (float)((tc >>  8) & 255) * light;
                float tb = (float)( tc         & 255) * light;

                float f = (riw - fogN) * fogI;
                if (f < 0.0f) f = 0.0f; else if (f > 1.0f) f = 1.0f;
                f *= f;
                int R = (int)(tr + (fR - tr) * f);
                int G = (int)(tg + (fG - tg) * f);
                int B = (int)(tb + (fB - tb) * f);
                if (R > 255) R = 255;
                if (G > 255) G = 255;
                if (B > 255) B = 255;

                g_pix++;
                drow[px] = iw;
                crow[px] = ((rr_u32)R << 16) | ((rr_u32)G << 8) | (rr_u32)B;
            } else g_zfail++;

            w0 += g0x; w1 += g1x; w2 += g2x;
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
 * below that, bare rock on the high ground.                              */
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
        /* Deterministic and spread over all eight values so every heading
         * is on screen at once.  A real game would set this when placing. */
        g_dir[x][y][z] = (unsigned char)((x * 2 + z * 3 + y) % 8);
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

    /* a few floating blocks -- their bottoms and headings are visible */
    g_type[24][12][24] = BT_ROCK;  g_dir[24][12][24] = 0;
    g_type[26][14][22] = BT_GRASS; g_dir[26][14][22] = 5;
    g_type[22][16][26] = BT_DIRT;  g_dir[22][16][26] = 3;
    g_type[30][13][30] = BT_ROCK;  g_dir[30][13][30] = 7;
}

/* ------------------------------------------------------------------ */
/* 5. baked face mesh                                                  */
/* ------------------------------------------------------------------ */

/* Only faces that touch air are stored.  With a static world this can be
 * built once, which turns every frame from "walk 55k voxels and test six
 * neighbours each" into "walk a few thousand faces".                     */
typedef struct MeshFace {
    float p[4][3];      /* world space corners                              */
    float n[3];
    float c[3];         /* centre, for backface + distance tests           */
    float du[4], dv[4]; /* decal coordinates (arrow or bar)                */
    float light;
    unsigned char type, dir, faceIdx, decal;
    float auv[4][2];   /* atlas uv; decals are already baked in        */
} MeshFace;

#define CH  8
#define NCX ((WX + CH - 1) / CH)
#define NCY ((WY + CH - 1) / CH)
#define NCZ ((WZ + CH - 1) / CH)
#define NCHUNK (NCX * NCY * NCZ)

static MeshFace* g_mesh = NULL;
static int g_meshN = 0, g_meshCap = 0;
static int g_chStart[NCHUNK], g_chCount[NCHUNK];

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
static const int FACE_D[6][3] = {
    { 0, 1, 0 }, { 0, -1, 0 }, { -1, 0, 0 }, { 1, 0, 0 }, { 0, 0, -1 }, { 0, 0, 1 }
};
/* Lighting: top brightest, bottom darkest.                              */
static const float FACE_L[6] = { 1.00f, 0.42f, 0.62f, 0.80f, 0.80f, 0.62f };

/* Texture uv per face corner.  On every side face u runs bottom -> top and
 * v runs along the horizontal edge (that is what puts the grass fringe on
 * the correct edge); on top and bottom both run in the horizontal plane. */
static const float FACE_U[6][4] = {
    { 0, 1, 1, 0 },   /* +Y: u along +Z, v along +X */
    { 0, 1, 1, 0 },   /* -Y */
    { 0, 0, 1, 1 },   /* -X: u is vertical */
    { 0, 0, 1, 1 },   /* +X */
    { 0, 0, 1, 1 },   /* -Z */
    { 0, 0, 1, 1 },   /* +Z */
};
static const float FACE_V[6][4] = {
    { 0, 0, 1, 1 },
    { 0, 0, 1, 1 },
    { 0, 1, 1, 0 },   /* sides: v follows the horizontal edge, u is height */
    { 0, 1, 1, 0 },
    { 0, 1, 1, 0 },
    { 0, 1, 1, 0 },
};
/* vertical coordinate of each corner: 0 at the block's floor, 1 at its top */
static const float FACE_VV[6][4] = {
    { 1, 1, 1, 1 }, { 0, 0, 0, 0 },
    { 0, 1, 1, 0 }, { 0, 1, 1, 0 }, { 0, 1, 1, 0 }, { 0, 1, 1, 0 }
};
/* horizontal coordinate along the side, 0..1                            */
static const float FACE_HH[6][4] = {
    { 0, 1, 1, 0 }, { 0, 1, 1, 0 },
    { 0, 0, 1, 1 }, { 0, 0, 1, 1 }, { 0, 0, 1, 1 }, { 0, 0, 1, 1 }
};

static void mesh_add_face(int x, int y, int z, int fi, unsigned char type, unsigned char dir) {
    MeshFace f;
    memset(&f, 0, sizeof(f));
    f.type = type; f.dir = dir; f.faceIdx = (unsigned char)fi;
    f.light = FACE_L[fi];
    float cx = 0, cy = 0, cz = 0;

    /* decal choice */
    f.decal = 0;
    float rot = 0.0f;
    if (fi == 0) {
        f.decal = 1;
        /* Top face uv: u along +Z (south), v along +X (east).  The arrow
         * stencil points at +v, i.e. east, so rotate it by -90 deg per step
         * of the heading to make it point at the block's direction.       */
        rot = -1.5707963f * (float)(dir % 4);
    } else if (fi >= 2 && fi == DIR_FACE[dir % 4]) {
        f.decal = 2;      /* the face this block is looking at gets a bar */
    }

    float cs = cosf(rot), sn = sinf(rot);
    for (int i = 0; i < 4; i++) {
        float px = (float)x + FACE_P[fi][i][0];
        float py = (float)y + FACE_P[fi][i][1];
        float pz = (float)z + FACE_P[fi][i][2];
        f.p[i][0] = px; f.p[i][1] = py; f.p[i][2] = pz;
        cx += px; cy += py; cz += pz;

        float u = FACE_U[fi][i], v = FACE_V[fi][i];
        if (f.decal == 1) {
            float ou = u - 0.5f, ov = v - 0.5f;
            f.du[i] = ou * cs - ov * sn + 0.5f;
            f.dv[i] = ou * sn + ov * cs + 0.5f;
        } else if (f.decal == 2) {
            f.du[i] = FACE_HH[fi][i];
            f.dv[i] = FACE_VV[fi][i];
        } else {
            f.du[i] = u; f.dv[i] = v;
        }
        {
            int ti = at_tile(type, fi, dir);
            int tc = ti % AT_COLS, tr = ti / AT_COLS;
            f.auv[i][0] = ((float)(tc*AT_TILE) + 0.5f + u*(AT_TILE-1)) / (float)AT_W;
            f.auv[i][1] = ((float)(tr*AT_TILE) + 0.5f + v*(AT_TILE-1)) / (float)AT_H;
        }
    }
    f.c[0] = cx * 0.25f; f.c[1] = cy * 0.25f; f.c[2] = cz * 0.25f;
    f.n[0] = (float)FACE_N[fi][0]; f.n[1] = (float)FACE_N[fi][1]; f.n[2] = (float)FACE_N[fi][2];
    mesh_push(&f);
}


/* Build every (type, face, heading) tile once.  The arrow and the bar are
 * drawn into the tile here, so the rasteriser's inner loop never branches. */
typedef struct Aff2 { float a, b, c, d, e, f; } Aff2;   /* du=a*u+b*v+c ... */

static Aff2 aff2_from(const float* U, const float* V,
                      const float* DU, const float* DV) {
    Aff2 r; memset(&r, 0, sizeof(r));
    float det = (U[1]-U[0])*(V[2]-V[0]) - (U[2]-U[0])*(V[1]-V[0]);
    if (fabsf(det) < 1e-9f) return r;
    float A = ((DU[1]-DU[0])*(V[2]-V[0]) - (DU[2]-DU[0])*(V[1]-V[0])) / det;
    float B = ((DU[2]-DU[0])*(U[1]-U[0]) - (DU[1]-DU[0])*(U[2]-U[0])) / det;
    float C = ((DV[1]-DV[0])*(V[2]-V[0]) - (DV[2]-DV[0])*(V[1]-V[0])) / det;
    float D = ((DV[2]-DV[0])*(U[1]-U[0]) - (DV[1]-DV[0])*(U[2]-U[0])) / det;
    r.a = A; r.b = B; r.c = DU[0] - A*U[0] - B*V[0];
    r.d = C; r.e = D; r.f = DV[0] - C*U[0] - D*V[0];
    return r;
}

#ifndef RR_HEADLESS
static IMAGE g_atlasImg;                 /* GPU copy of the atlas            */
#endif

static void atlas_build(void) {
    g_atlas.w = AT_W; g_atlas.h = AT_H; g_atlas.size = AT_W;
    g_atlas.px = (rr_u32*)malloc((size_t)AT_W * AT_H * sizeof(rr_u32));
    if (!g_atlas.px) { fprintf(stderr, "out of memory\n"); exit(1); }
    memset(g_atlas.px, 0, (size_t)AT_W * AT_H * sizeof(rr_u32));

    for (int t = 1; t < BT_N; t++)
    for (int fi = 0; fi < 6; fi++)
    for (int d = 0; d < 8; d++) {
        int ti  = at_tile(t, fi, d);
        int ox  = (ti % AT_COLS) * AT_TILE;
        int oy  = (ti / AT_COLS) * AT_TILE;
        const Tex* base = &g_btex[t][FACE_TEX[fi]];

        int decal = 0; float rot = 0.0f;
        if (fi == 0) { decal = 1; rot = -1.5707963f * (float)(d % 4); }
        else if (fi >= 2 && fi == DIR_FACE[d % 4]) decal = 2;

        int   up = (d < 4);
        float dr = up ? 244.0f : 26.0f;
        float mlo = up ? 0.44f : -0.02f, mhi = up ? 1.02f : 0.56f;
        float cs = cosf(rot), sn = sinf(rot);

        Aff2 af; memset(&af, 0, sizeof(af));
        if (decal == 2)
            af = aff2_from(FACE_U[fi], FACE_V[fi], FACE_HH[fi], FACE_VV[fi]);

        for (int y = 0; y < AT_TILE; y++)
        for (int x = 0; x < AT_TILE; x++) {
            float u = ((float)x + 0.5f) / (float)AT_TILE;
            float v = ((float)y + 0.5f) / (float)AT_TILE;
            rr_u32 tc = tex_sample(base, u, v);
            float tr = (float)((tc >> 16) & 255);
            float tg = (float)((tc >>  8) & 255);
            float tb = (float)( tc         & 255);
            if (decal) {
                int hit = 0;
                if (decal == 1) {
                    float ou = u - 0.5f, ov = v - 0.5f;
                    hit = arrow_mask(ou*cs - ov*sn + 0.5f, ou*sn + ov*cs + 0.5f);
                } else {
                    float du = af.a*u + af.b*v + af.c;
                    float dv = af.d*u + af.e*v + af.f;
                    hit = (fabsf(du - 0.5f) < 0.14f && dv >= mlo && dv <= mhi);
                }
                if (hit) {
                    tr += (dr - tr) * 0.78f;
                    tg += (dr - tg) * 0.78f;
                    tb += (dr - tb) * 0.78f;
                }
            }
            g_atlas.px[(size_t)(oy + y) * AT_W + (ox + x)] =
                ((rr_u32)(int)tr << 16) | ((rr_u32)(int)tg << 8) | (rr_u32)(int)tb;
        }
    }
#ifndef RR_HEADLESS
    /* The same pixels again as an easygl IMAGE for the GPU path.  Two
     * conventions have to be satisfied at once:
     *   - easygl stores 0x00BBGGRR, hence the swapped byte order;
     *   - an IMAGE's texture is bottom up, so texture coordinate v = 0 is
     *     the LAST row of the buffer GetImageBuffer() hands back, while the
     *     CPU rasteriser's v = 0 is the FIRST row of g_atlas.
     * Writing the rows top down therefore mirrors the atlas vertically and
     * every tile samples from row (AT_ROWS-1-tr).  Only tiles 0..143 are
     * baked, so anything above row 8 comes back black -- which is exactly
     * the "everything is black except the stone's four side faces" bug:
     * those four sit in rows 7 and 8, the only pair that mirrors onto each
     * other, and all four are the same grey so the swap went unnoticed.  */
    Resize(&g_atlasImg, AT_W, AT_H);
    DWORD* dst = GetImageBuffer(&g_atlasImg);
    if (dst)
        for (int y = 0; y < AT_H; y++) {
            const rr_u32* src = g_atlas.px + (size_t)y * AT_W;
            DWORD* out = dst + (size_t)(AT_H - 1 - y) * AT_W;
            for (int x = 0; x < AT_W; x++) {
                rr_u32 c = src[x];
                out[x] = (DWORD)(((c & 255u) << 16) | (c & 0xFF00u) | ((c >> 16) & 255u));
            }
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
        int x0 = cx * CH, y0 = cy * CH, z0 = cz * CH;
        for (int x = x0; x < x0 + CH && x < WX; x++)
        for (int y = y0; y < y0 + CH && y < WY; y++)
        for (int z = z0; z < z0 + CH && z < WZ; z++) {
            unsigned char t = g_type[x][y][z];
            if (t == BT_AIR) continue;
            unsigned char d = g_dir[x][y][z];
            for (int fi = 0; fi < 6; fi++) {
                int nx = x + FACE_D[fi][0], ny = y + FACE_D[fi][1], nz = z + FACE_D[fi][2];
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
    /* left   = w + x */  p[0].a = a[12]+a[0];  p[0].b = a[13]+a[1];  p[0].c = a[14]+a[2];  p[0].d = a[15]+a[3];
    /* right  = w - x */  p[1].a = a[12]-a[0];  p[1].b = a[13]-a[1];  p[1].c = a[14]-a[2];  p[1].d = a[15]-a[3];
    /* bottom = w + y */  p[2].a = a[12]+a[4];  p[2].b = a[13]+a[5];  p[2].c = a[14]+a[6];  p[2].d = a[15]+a[7];
    /* top    = w - y */  p[3].a = a[12]-a[4];  p[3].b = a[13]-a[5];  p[3].c = a[14]-a[6];  p[3].d = a[15]-a[7];
    /* near   = w + z */  p[4].a = a[12]+a[8];  p[4].b = a[13]+a[9];  p[4].c = a[14]+a[10]; p[4].d = a[15]+a[11];
    /* far    = w - z */  p[5].a = a[12]-a[8];  p[5].b = a[13]-a[9];  p[5].c = a[14]-a[10]; p[5].d = a[15]-a[11];
}

static int aabb_in_frustum(const Plane* p, float mnx, float mny, float mnz,
                           float mxx, float mxy, float mxz) {
    for (int i = 0; i < 6; i++) {
        /* the corner furthest along the plane normal */
        float px = p[i].a >= 0 ? mxx : mnx;
        float py = p[i].b >= 0 ? mxy : mny;
        float pz = p[i].c >= 0 ? mxz : mnz;
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
     * its normal points to.  Robust -- independent of vertex ordering.     */
    float ex = eye->x - f->c[0], ey = eye->y - f->c[1], ez = eye->z - f->c[2];
    if (f->n[0] * ex + f->n[1] * ey + f->n[2] * ez <= 0.0f) { g_backface++; return; }

    /* The heading arrow and the facing side bar are already baked into the
     * atlas tile this face samples, so there is no per pixel decal work.   */
    CVtx clip[4];
    for (int i = 0; i < 4; i++) {
        float px = f->p[i][0], py = f->p[i][1], pz = f->p[i][2];
        /* ROW MAJOR: row r lives in m[r*4 + 0..3].                        */
        clip[i].x = mvp->m[0]  * px + mvp->m[1]  * py + mvp->m[2]  * pz + mvp->m[3];
        clip[i].y = mvp->m[4]  * px + mvp->m[5]  * py + mvp->m[6]  * pz + mvp->m[7];
        clip[i].z = mvp->m[8]  * px + mvp->m[9]  * py + mvp->m[10] * pz + mvp->m[11];
        clip[i].w = mvp->m[12] * px + mvp->m[13] * py + mvp->m[14] * pz + mvp->m[15];
        clip[i].u = f->auv[i][0];
        clip[i].v = f->auv[i][1];
        clip[i].du = f->du[i];
        clip[i].dv = f->dv[i];
        clip[i].light = f->light;
    }

    CVtx tri[3] = { clip[0], clip[1], clip[2] };
    CVtx buf[8];
    int n = clip_near(tri, 3, buf, RR_NEAR);
    if (n < 3) g_clipped++;
    for (int i = 0; i + 2 < n; i++) {
        CVtx t[3] = { buf[0], buf[i + 1], buf[i + 2] };
        raster_tri(t[0], t[1], t[2], f->light);
    }
    tri[0] = clip[0]; tri[1] = clip[2]; tri[2] = clip[3];
    n = clip_near(tri, 3, buf, RR_NEAR);
    for (int i = 0; i + 2 < n; i++) {
        CVtx t[3] = { buf[0], buf[i + 1], buf[i + 2] };
        raster_tri(t[0], t[1], t[2], f->light);
    }
}

static void world_draw(const M4* mvp, const V3* eye, const V3* fwd) {
    (void)fwd;
    Plane pl[6];
    planes_from_mvp(mvp, pl);

    for (int ci = 0; ci < NCHUNK; ci++) {
        if (g_chCount[ci] == 0) continue;
        int cx = ci % NCX, cy = (ci / NCX) % NCY, cz = ci / (NCX * NCY);
        float mnx = (float)(cx * CH), mny = (float)(cy * CH), mnz = (float)(cz * CH);
        float mxx = mnx + CH, mxy = mny + CH, mxz = mnz + CH;

        if (!aabb_in_frustum(pl, mnx, mny, mnz, mxx, mxy, mxz)) { g_chunkOut++; continue; }
        /* chunk centre vs fog distance */
        float dx = (mnx + mxx) * 0.5f - eye->x;
        float dy = (mny + mxy) * 0.5f - eye->y;
        float dz = (mnz + mxz) * 0.5f - eye->z;
        float dd = sqrtf(dx * dx + dy * dy + dz * dz);
        if (dd - CH > RR_CULL_DIST) { g_farout++; continue; }

        g_chunkIn++;
        int s = g_chStart[ci], e = s + g_chCount[ci];
        for (int i = s; i < e; i++) face_emit(&g_mesh[i], mvp, eye);
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
 * be read off the title bar while walking around.                          */
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

/* ------------------------------------------------------------------ */
/* 7. one frame                                                        */
/* ------------------------------------------------------------------ */

/* Declared here so render_frame() can dispatch to it; defined in 7b, and
 * in the headless build it is a stub that always returns 0.               */
static int glr_frame(const M4* mvp);

/* F9 flips this at run time so a broken shader path can be confirmed
 * without rebuilding.                                                     */
static int g_forceCpu = 0;

/* The mesh path draws through easygl's createmesh/meshdraw.  It is the one
 * that actually beats Minecraft: geometry is uploaded once at load time and
 * every frame after that is a handful of draw calls with no per pixel CPU
 * work.  If it fails the frame falls back to the CPU rasteriser instead of
 * leaving a blank window.                                                */
#ifndef GPUMESH_ENABLE
#define GPUMESH_ENABLE   1
#endif

/* Returns 1 when the shader path produced the frame, 0 when the CPU
 * rasteriser did.  The caller must not blit the CPU buffer over a frame
 * the GPU already owns.                                                  */
/* ------------------------------------------------------------------ */
/* 7c. The fast path: real GPU geometry through easygl's mesh API      */
/* ------------------------------------------------------------------ */
#ifndef RR_HEADLESS

static GXMESH* g_chMesh[NCHUNK];
static IMAGE   g_skyImg;
static int     g_meshReady = 0;
static int     g_chunksDrawn = 0;

/* A 1 x 256 gradient, stretched over the canvas with one putimage().  The
 * mesh pass clears to alpha 0 and blends, so this stays visible wherever
 * no geometry was drawn -- the sky costs one quad, not 921600 stores. */
static void sky_build(void) {
    Resize(&g_skyImg, 1, 256);
    DWORD* d = GetImageBuffer(&g_skyImg);
    if (!d) return;
    for (int i = 0; i < 256; i++) {
        float t = (float)i / 255.0f;                 /* 0 at the top */
        int r = (int)( 46.0f + (150.0f -  46.0f) * t + 0.5f);
        int g = (int)( 96.0f + (190.0f -  96.0f) * t + 0.5f);
        int b = (int)(168.0f + (235.0f - 168.0f) * t + 0.5f);
        d[i] = RGB(r, g, b);
    }
}

/* End to end check of the atlas upload: pull one pixel per (type, face)
 * back out of the GPU image and compare it with the CPU copy the software
 * rasteriser samples.  A mismatch means the two paths disagree, and an
 * orientation flip is exactly what that looks like -- tiles read from the
 * wrong row, which once left every face black except the stone's four
 * sides.  Say so and let the CPU rasteriser draw rather than show a frame
 * that is wrong in a way nobody can diagnose from the picture alone.     */
static int atlas_verify(void) {
    int bad = 0;
    if (!isglready()) return 1;
    SetWorkingImage(&g_atlasImg);
    for (int t = 1; t < BT_N && !bad; t++)
        for (int fi = 0; fi < 6 && !bad; fi++) {
            int ti = at_tile(t, fi, 0);
            int ox = (ti % AT_COLS) * AT_TILE + AT_TILE / 2;
            int oy = (ti / AT_COLS) * AT_TILE + AT_TILE / 2;
            COLORREF got = getpixel(ox, oy);
            rr_u32 c = g_atlas.px[(size_t)oy * AT_W + ox];
            int gr = (int)GetRValue(got), gg = (int)GetGValue(got), gb = (int)GetBValue(got);
            int er = (int)((c >> 16) & 255), eg = (int)((c >> 8) & 255), eb = (int)(c & 255);
            if (gr != er || gg != eg || gb != eb) {
                fprintf(stderr,
                    "gpumesh: atlas mismatch, tile %d (%s %s) at (%d,%d): "
                    "GPU=(%d,%d,%d) CPU=(%d,%d,%d)\n",
                    ti, BT_NAME[t],
                    fi == 0 ? "top" : (fi == 1 ? "bottom" : "side"),
                    ox, oy, gr, gg, gb, er, eg, eb);
                bad = 1;
            }
        }
    SetWorkingImage(NULL);
    return !bad;
}

/* One mesh per chunk, built once.  Uploading is the expensive part and it
 * happens here, at load time, never per frame.  A chunk that changes later
 * is re-uploaded with updatemesh() rather than rebuilt from scratch. */
static void gpumesh_build(void) {
    if (!atlas_verify()) { g_meshReady = 0; return; }
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
                o[3] = f->auv[k][0]; o[4] = f->auv[k][1];
                o[5] = o[6] = o[7] = f->light;
            }
            unsigned int b = (unsigned int)(i * 4);
            ix[i*6+0] = b;     ix[i*6+1] = b + 1; ix[i*6+2] = b + 2;
            ix[i*6+3] = b;     ix[i*6+4] = b + 2; ix[i*6+5] = b + 3;
        }
        g_chMesh[ci] = createmesh(v, n * 4, ix, n * 6);
        free(v); free(ix);
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
    float cm[16];
    if (!GPUMESH_ENABLE) return 0;
    if (!g_meshReady) return 0;

    m4_to_colmajor(mvp, cm);

    /* The sky is an ordinary 2D draw, so it has to go down before the 3D
     * pass opens; meshbegin() flushes the batch for us. */
    putimage(0, 0, g_fb.w, g_fb.h, &g_skyImg, 0, 0);

    if (!meshbegin()) return 0;

    meshfog(RR_FOG_NEAR, RR_FOG_FAR, RGB(150, 190, 235));
    mesheye(eye.x, eye.y, eye.z);

    g_chunksDrawn = 0;
    for (int ci = 0; ci < NCHUNK; ci++) {
        if (!g_chMesh[ci]) continue;
        int cx = ci % NCX, cy = (ci / NCX) % NCY, cz = ci / (NCX * NCY);
        float mnx = (float)(cx * CH), mny = (float)(cy * CH), mnz = (float)(cz * CH);
        /* distance cull first -- it is the cheaper of the two tests */
        float dx = mnx + CH * 0.5f - eye.x;
        float dy = mny + CH * 0.5f - eye.y;
        float dz = mnz + CH * 0.5f - eye.z;
        if (dx*dx + dy*dy + dz*dz > (RR_CULL_DIST + CH) * (RR_CULL_DIST + CH)) continue;
        if (!meshvisible(cm, mnx, mny, mnz, mnx + CH, mny + CH, mnz + CH)) continue;
        meshdraw(g_chMesh[ci], cm, &g_atlasImg);
        g_chunksDrawn++;
    }
    meshend();
    /* Nothing survived culling on a world that certainly has geometry: that
     * is a broken pass, not an empty view, so hand the frame to the CPU
     * rasteriser rather than showing sky over a scene that should be there. */
    if (g_chunksDrawn == 0 && g_meshN > 0) {
        fprintf(stderr, "gpumesh: 0 chunks drawn, falling back to CPU\n");
        return 0;
    }
    return 1;
}

#endif /* !RR_HEADLESS */

static int render_frame(void) {
    float fov = 1.2217f;                /* 70 degrees */
    float aspect = (float)g_fb.w / (float)g_fb.h;

    M4 proj = m4persp(fov, aspect, RR_NEAR, RR_FAR);
    V3 eye = cam_eye(&g_cam);
    V3 fwd = cam_forward(&g_cam);
    M4 view = m4view(eye, fwd, v3(0, 1, 0));
    M4 mvp = m4mul(&proj, &view);

    /* fog: express distance <-> ndc z with the exact projection constants */
    /* z_ndc = -m10 + m11 / d,  so  d = m11 / (z_ndc + m10)                */
    g_fogA = -proj.m[10];
    g_fogB = proj.m[11];

    /* One decision point: if the GPU path produced the frame, the CPU
     * rasteriser never runs at all.                                      */
#ifndef RR_HEADLESS
    /* Geometry first: if the driver can do it, this frame costs a
     * handful of draw calls and no per pixel CPU work at all. */
    if (!g_forceCpu && gpu_mesh_frame(&mvp, eye)) return 1;
#endif
    if (!g_forceCpu && glr_frame(&mvp)) return 1;

    fb_sky();
    fb_clear_depth();                   /* <- every frame, no exceptions   */
    world_draw(&mvp, &eye, &fwd);
    return 0;
}

/* Convert our 0x00RRGGBB buffer into whatever the display layer wants.
 * easygl's GetImageBuffer() hands back 0x00BBGGRR.                       */
static void present(const rr_u32* src, rr_u32* dst, int n) {
    for (int i = 0; i < n; i++) {
        rr_u32 c = src[i];
        int r = (int)((c >> 16) & 255), g = (int)((c >> 8) & 255), b = (int)(c & 255);
        dst[i] = ((rr_u32)b << 16) | ((rr_u32)g << 8) | (rr_u32)r;
    }
}

/* ------------------------------------------------------------------ */
/* 7b. GPU shader path                                                 */
/* ------------------------------------------------------------------ */

/* Why this exists: the CPU path pays for every pixel three times over --
 * once to rasterise it, once to shuffle 0x00RRGGBB into the display's
 * 0x00BBGGRR, and once to upload the finished frame as a texture.  The
 * world is static, so all of that can be done once at load time and
 * replayed by the GPU with a handful of draw calls.
 *
 * easygl.h is #included into this translation unit, so its file scope
 * statics are visible here.  That gives us getcanvastex(): we render
 * into our own framebuffer and copy the result straight into the canvas
 * texture, after which easygl's normal present blits it unchanged.  No
 * upload, no per pixel CPU work.
 *
 * easygl also already loaded nearly every entry point we need as a static
 * function pointer, so those are reused rather than fetched again.       */
#ifndef RR_HEADLESS

/* OFF by default.  The shader path cannot be verified without a real GL
 * context, and a silent failure there shows up as a blank window, so the
 * CPU rasteriser owns the frame until this is turned on (or F9 is pressed). */
#define GLR_ENABLE   0    /* set to 1 to try the shader path */

#ifndef GL_DEPTH_COMPONENT24
#define GL_DEPTH_COMPONENT24   0x81A6
#endif
#ifndef GL_DEPTH_COMPONENT16
#define GL_DEPTH_COMPONENT16   0x81A5
#endif
#ifndef GL_DEPTH_ATTACHMENT
#define GL_DEPTH_ATTACHMENT    0x8D00
#endif
#ifndef GL_STATIC_DRAW
#define GL_STATIC_DRAW         0x88E4
#endif
#ifndef GL_FRAMEBUFFER_BINDING
#define GL_FRAMEBUFFER_BINDING 0x8CA6
#endif
#ifndef GL_CURRENT_PROGRAM
#define GL_CURRENT_PROGRAM     0x8B8D
#endif
#ifndef GL_VERTEX_ARRAY_BINDING
#define GL_VERTEX_ARRAY_BINDING 0x85B5
#endif
#ifndef GL_ARRAY_BUFFER_BINDING
#define GL_ARRAY_BUFFER_BINDING 0x8896
#endif
#ifndef GL_TEXTURE_BINDING_2D
#define GL_TEXTURE_BINDING_2D  0x8069
#endif
#ifndef GL_VIEWPORT
#define GL_VIEWPORT            0x0BA2
#endif
#ifndef GL_DEPTH_WRITEMASK
#define GL_DEPTH_WRITEMASK     0x0B72
#endif
#ifndef GL_COLOR_CLEAR_VALUE
#define GL_COLOR_CLEAR_VALUE   0x0C22
#endif
/* MinGW ships a GL 1.1 gl.h that defines GL_TEXTURE0 but not the rest, so
 * the second unit has to be declared here (see the same note in easygl).  */
#ifndef GL_TEXTURE1
#define GL_TEXTURE1            0x84C1
#endif

typedef void (*P_genVAO)(GLsizei, GLuint*);
typedef void (*P_bindVAO)(GLuint);
typedef void (*P_rbStore)(GLenum, GLenum, GLsizei, GLsizei);

static P_genVAO glrGenVAO = 0;
static P_bindVAO glrBindVAO = 0;
static P_rbStore glrRBStore = 0;

/* #version 120: easygl runs a compatibility profile and its own shaders
 * are 120, so attribute/varying/texture2D is what the driver accepts.   */
static const char* GLR_VS =
    "#version 120\n"
    "attribute vec3 aPos;\n"
    "attribute vec2 aUv;\n"
    "attribute vec2 aDc;\n"
    "attribute float aLit;\n"
    "uniform mat4 uMvp;\n"
    "varying vec2 vUv; varying vec2 vDc; varying float vLit; varying float vDist;\n"
    "void main(void){\n"
    "    gl_Position = uMvp * vec4(aPos, 1.0);\n"
    "    vUv = aUv; vDc = aDc; vLit = aLit;\n"
    "    vDist = gl_Position.w;\n"      /* w_clip == view space distance */
    "}\n";

static const char* GLR_FS =
    "#version 120\n"
    "uniform sampler2D uAlb;\n"
    "uniform sampler2D uDc;\n"
    "uniform vec3 uFog;\n"
    "uniform vec2 uFogR;\n"
    "varying vec2 vUv; varying vec2 vDc; varying float vLit; varying float vDist;\n"
    "void main(void){\n"
    "    vec3 c = texture2D(uAlb, vUv).rgb * vLit;\n"
    "    vec4 d = texture2D(uDc, vDc);\n"
    "    c = mix(c, d.rgb, d.a * 0.78);\n"
    "    float f = (vDist - uFogR.x) / (uFogR.y - uFogR.x);\n"
    "    f = clamp(f, 0.0, 1.0);\n"
    "    f = f * f;\n"
    "    gl_FragColor = vec4(mix(c, uFog, f), 1.0);\n"
    "}\n";

static const char* GLR_SKY_VS =
    "#version 120\n"
    "attribute vec2 aP;\n"
    "varying float vT;\n"
    "void main(void){\n"
    "    gl_Position = vec4(aP, 0.0, 1.0);\n"
    "    vT = (1.0 - aP.y) * 0.5;\n"   /* 0 at the top of the screen */
    "}\n";

static const char* GLR_SKY_FS =
    "#version 120\n"
    "uniform vec3 uA; uniform vec3 uB;\n"
    "varying float vT;\n"
    "void main(void){ gl_FragColor = vec4(mix(uA, uB, vT), 1.0); }\n";

typedef struct GVert { float x, y, z; float u, v; float du, dv; float lit; } GVert;

static GLuint glrProg = 0, glrSkyProg = 0;
static GLuint glrVao = 0, glrVbo = 0, glrSkyVao = 0, glrSkyVbo = 0;
static GLuint glrAlb = 0, glrDec = 0;
static GLuint glrFbo = 0, glrCol = 0, glrDepth = 0;
static int    glrW = 0, glrH = 0, glrReady = 0;
static GLsizei glrVertN = 0;
static int    glr_uMvp = -1, glr_uAlb = -1, glr_uDc = -1;
static int    glr_uFog = -1, glr_uFogR = -1;
static int    glr_suA = -1, glr_suB = -1;

static GLuint glr_shader(GLenum type, const char* src) {
    GLuint s = glCreateShader(type);
    GLint ok = 0;
    char log[2048];
    log[0] = 0;
    if (!s) return 0;
    glShaderSource(s, 1, &src, NULL);
    glCompileShader(s);
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        glGetShaderInfoLog(s, (GLsizei)sizeof(log) - 1, NULL, log);
        fprintf(stderr, "glr: shader compile failed:\n%s\n", log);
        glDeleteShader(s);
        return 0;
    }
    return s;
}

/* GLSL 120 has no layout qualifier, so an attribute lands wherever the
 * driver feels like putting it unless we pin it before linking.  We feed
 * slots 3..6 and the driver would happily hand us 0..3 instead, which is
 * exactly why the first attempt drew nothing: every vertex read the
 * disabled generic attribute default (0,0,0,1) and collapsed to a point. */
static GLuint glr_program(const char* vs, const char* fs,
                          const char** names, int nname, int base) {
    GLuint v = glr_shader(GL_VERTEX_SHADER, vs);
    GLuint f = glr_shader(GL_FRAGMENT_SHADER, fs);
    GLuint p = 0;
    GLint ok = 0;
    char log[2048];
    log[0] = 0;
    if (!v || !f) { if (v) glDeleteShader(v); if (f) glDeleteShader(f); return 0; }
    p = glCreateProgram();
    if (!p) { glDeleteShader(v); glDeleteShader(f); return 0; }
    glAttachShader(p, v);
    glAttachShader(p, f);
    if (glBindAttribLocation) {
        for (int i = 0; i < nname; i++)
            glBindAttribLocation(p, (GLuint)(base + i), names[i]);
    }
    glLinkProgram(p);
    glDeleteShader(v);
    glDeleteShader(f);
    glGetProgramiv(p, GL_LINK_STATUS, &ok);
    if (!ok) {
        glGetProgramInfoLog(p, (GLsizei)sizeof(log) - 1, NULL, log);
        fprintf(stderr, "glr: link failed:\n%s\n", log);
        glDeleteProgram(p);
        return 0;
    }
    return p;
}

/* --- texture atlases ------------------------------------------------ */

static void glr_atlas_albedo(void) {
    /* 4 x 3 tiles of 64x64.  tile = type * 3 + slot, matching FACE_TEX.  */
    const int TW = 4, TH = 3, S = 64;
    unsigned char* px = (unsigned char*)calloc((size_t)(TW * S) * (TH * S), 4);
    if (!px) return;
    for (int t = 0; t < BT_N; t++) {
        for (int s = 0; s < 3; s++) {
            Tex* src = &g_btex[t][s];
            if (!src->px || src->size != S) continue;
            int tx = (t * 3 + s) % TW, ty = (t * 3 + s) / TW;
            for (int y = 0; y < S; y++) {
                for (int x = 0; x < S; x++) {
                    rr_u32 c = src->px[(size_t)y * S + x];
                    /* glTexImage2D() takes its first row as the BOTTOM of
                     * the texture, so a top down block mirrors the atlas
                     * vertically -- the same flip the IMAGE upload above
                     * has to undo.                                       */
                    size_t o = ((size_t)(TH * S - 1 - (ty * S + y)) * (TW * S)
                                + (tx * S + x)) * 4;
                    px[o + 0] = (unsigned char)((c >> 16) & 255);
                    px[o + 1] = (unsigned char)((c >> 8) & 255);
                    px[o + 2] = (unsigned char)(c & 255);
                    px[o + 3] = 255;
                }
            }
        }
    }
    glGenTextures(1, &glrAlb);
    glBindTexture(GL_TEXTURE_2D, glrAlb);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, TW * S, TH * S, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, px);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
    free(px);
}

static void glr_atlas_decal(void) {
    /* 4 x 4 tiles of 64x64: index = dir * 2 + (arrow ? 0 : 1).
     * Baked with exactly the tests the CPU rasteriser uses, so switching
     * paths does not change how a block's heading looks.                 */
    const int TW = 4, TH = 4, S = 64;
    unsigned char* px = (unsigned char*)calloc((size_t)(TW * S) * (TH * S), 4);
    if (!px) return;
    for (int dir = 0; dir < 8; dir++) {
        int up = (dir < 4);
        float mlo = up ? 0.44f : -0.02f;
        float mhi = up ? 1.02f : 0.56f;
        for (int kind = 0; kind < 2; kind++) {
            int idx = dir * 2 + kind;
            int tx = idx % TW, ty = idx / TW;
            for (int y = 0; y < S; y++) {
                for (int x = 0; x < S; x++) {
                    float du = ((float)x + 0.5f) / (float)S;
                    float dv = ((float)y + 0.5f) / (float)S;
                    int hit = kind == 0 ? arrow_mask(du, dv)
                                        : (fabsf(du - 0.5f) < 0.14f && dv >= mlo && dv <= mhi);
                    size_t o = ((size_t)(TH * S - 1 - (ty * S + y)) * (TW * S)
                                + (tx * S + x)) * 4;
                    px[o + 0] = px[o + 1] = px[o + 2] = (unsigned char)(up ? 244 : 26);
                    px[o + 3] = hit ? 255 : 0;
                }
            }
        }
    }
    glGenTextures(1, &glrDec);
    glBindTexture(GL_TEXTURE_2D, glrDec);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, TW * S, TH * S, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, px);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
    free(px);
}

/* --- static geometry ------------------------------------------------ */

static void glr_mesh(void) {
    GVert* v = (GVert*)malloc((size_t)g_meshN * 6 * sizeof(GVert));
    if (!v) return;
    GLsizei n = 0;
    for (int i = 0; i < g_meshN; i++) {
        const MeshFace* f = &g_mesh[i];
        int tile = (int)f->type * 3 + FACE_TEX[f->faceIdx];
        float au = (float)(tile % 4) * 0.25f, av = (float)(tile / 4) * (1.0f / 3.0f);
        int dti = (int)f->dir * 2 + (f->decal == 1 ? 0 : 1);
        float du0 = (float)(dti % 4) * 0.25f, dv0 = (float)(dti / 4) * 0.25f;
        static const int QUAD[6] = { 0, 1, 2, 0, 2, 3 };
        for (int q = 0; q < 6; q++) {
            int c = QUAD[q];
            GVert* g = &v[n++];
            g->x = f->p[c][0]; g->y = f->p[c][1]; g->z = f->p[c][2];
            g->u  = au  + FACE_U[f->faceIdx][c] * 0.25f;
            g->v  = av  + FACE_V[f->faceIdx][c] * (1.0f / 3.0f);
            g->du = du0 + f->du[c] * 0.25f;
            g->dv = dv0 + f->dv[c] * 0.25f;
            g->lit = f->light;
        }
    }
    glrVertN = n;

    /* Our own VAO keeps easygl's enabled attribs 0/1/2 untouched: binding
     * it switches in a completely separate vertex state, so there is never
     * a moment where the driver reads easygl's vertex buffer with our
     * vertex count (the out of range read that made the earlier attempt
     * render nothing at all).                                           */
    glrGenVAO(1, &glrVao);
    glrBindVAO(glrVao);
    glGenBuffers(1, &glrVbo);
    glBindBuffer(GL_ARRAY_BUFFER, glrVbo);
    glBufferData(GL_ARRAY_BUFFER, (GLsizei)(n * (GLsizei)sizeof(GVert)), v, GL_STATIC_DRAW);
    glEnableVertexAttribArray(3);
    glEnableVertexAttribArray(4);
    glEnableVertexAttribArray(5);
    glEnableVertexAttribArray(6);
    glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, sizeof(GVert), (const void*)0);
    glVertexAttribPointer(4, 2, GL_FLOAT, GL_FALSE, sizeof(GVert), (const void*)12);
    glVertexAttribPointer(5, 2, GL_FLOAT, GL_FALSE, sizeof(GVert), (const void*)20);
    glVertexAttribPointer(6, 1, GL_FLOAT, GL_FALSE, sizeof(GVert), (const void*)28);
    glrBindVAO(0);

    static const float sky[12] = { -1,-1, 3,-1, -1, 3, -1, 3, 3,-1, 3, 3 };
    glrGenVAO(1, &glrSkyVao);
    glrBindVAO(glrSkyVao);
    glGenBuffers(1, &glrSkyVbo);
    glBindBuffer(GL_ARRAY_BUFFER, glrSkyVbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(sky), sky, GL_STATIC_DRAW);
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, 8, (const void*)0);
    glrBindVAO(0);
    free(v);
}

static int glr_targets(int w, int h) {
    if (glrFbo && glrW == w && glrH == h) return 1;
    if (glrFbo) {
        glDeleteFramebuffers(1, &glrFbo);
        glDeleteRenderbuffers(1, &glrDepth);
        glDeleteTextures(1, &glrCol);
        glrFbo = glrDepth = glrCol = 0;
    }
    glGenTextures(1, &glrCol);
    glBindTexture(GL_TEXTURE_2D, glrCol);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    glGenRenderbuffers(1, &glrDepth);
    glBindRenderbuffer(GL_RENDERBUFFER, glrDepth);
    /* 24 bit first, 16 bit if the driver refuses.  A missing depth buffer
     * would silently destroy the whole scene, so this one is fatal.       */
    glrRBStore(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, w, h);
    glGenFramebuffers(1, &glrFbo);
    glBindFramebuffer(GL_FRAMEBUFFER, glrFbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, glrCol, 0);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, glrDepth);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        glrRBStore(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, w, h);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, glrDepth);
        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
            fprintf(stderr, "glr: framebuffer incomplete\n");
            return 0;
        }
    }
    glrW = w; glrH = h;
    return 1;
}

static void glr_init(void) {
    if (!GLR_ENABLE) return;
    if (!isglready()) return;
    glrGenVAO  = (P_genVAO)wglGetProcAddress("glGenVertexArrays");
    glrBindVAO = (P_bindVAO)wglGetProcAddress("glBindVertexArray");
    glrRBStore = (P_rbStore)wglGetProcAddress("glRenderbufferStorage");
    if (!glrGenVAO || !glrBindVAO) glrGenVAO = (P_genVAO)wglGetProcAddress("glGenVertexArraysAPPLE");
    if (!glrGenVAO || !glrBindVAO || !glrRBStore) {
        fprintf(stderr, "glr: no VAO / renderbuffer storage, using CPU path\n");
        return;
    }
    if (!glBindAttribLocation) {
        fprintf(stderr, "glr: no glBindAttribLocation, using CPU path\n");
        return;
    }
    {
        static const char* mn[4] = { "aPos", "aUv", "aDc", "aLit" };
        static const char* sn[1] = { "aP" };
        glrProg    = glr_program(GLR_VS, GLR_FS, mn, 4, 3);
        glrSkyProg = glr_program(GLR_SKY_VS, GLR_SKY_FS, sn, 1, 3);
    }
    if (!glrProg || !glrSkyProg) return;
    glUseProgram(glrProg);
    glr_uMvp = glGetUniformLocation(glrProg, "uMvp");
    glr_uAlb = glGetUniformLocation(glrProg, "uAlb");
    glr_uDc  = glGetUniformLocation(glrProg, "uDc");
    glr_uFog = glGetUniformLocation(glrProg, "uFog");
    glr_uFogR = glGetUniformLocation(glrProg, "uFogR");
    glUniform1i(glr_uAlb, 0);
    glUniform1i(glr_uDc, 1);
    glUniform3f(glr_uFog, 150.0f / 255.0f, 185.0f / 255.0f, 225.0f / 255.0f);
    glUniform2f(glr_uFogR, RR_FOG_NEAR, RR_FOG_FAR);
    glUseProgram(glrSkyProg);
    glr_suA = glGetUniformLocation(glrSkyProg, "uA");
    glr_suB = glGetUniformLocation(glrSkyProg, "uB");
    glUniform3f(glr_suA, 46.0f / 255.0f, 96.0f / 255.0f, 168.0f / 255.0f);
    glUniform3f(glr_suB, 134.0f / 255.0f, 200.0f / 255.0f, 230.0f / 255.0f);
    glUseProgram(0);

    glr_atlas_albedo();
    glr_atlas_decal();
    glr_mesh();
    if (!glr_targets(getcanvaswidth(), getcanvasheight())) return;
    glrReady = 1;
    fprintf(stderr, "glr: GPU path active, %d triangles\n", (int)(glrVertN / 3));
}

/* One readback, run once.  The top of the frame is always sky, so if it is
 * neither blue nor anything at all, the pass drew nothing -- the classic
 * symptom of an attribute that landed on a slot we never filled.          */
static int glrChecked = 0;

static int glr_verify(void) {
    unsigned char px[4 * 8];
    int x = glrW / 2, y = glrH - 8;
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    glReadPixels(x, y, 2, 4, GL_RGBA, GL_UNSIGNED_BYTE, px);
    int blue = 0, blank = 0;
    for (int i = 0; i < 8; i++) {
        int r = px[i * 4 + 0], g = px[i * 4 + 1], b = px[i * 4 + 2];
        if (b > r + 10 && b > 40) blue++;
        if (r < 6 && g < 6 && b < 6) blank++;
    }
    /* nothing drawn at all, or the sky pass never happened */
    if (blank == 8 || blue < 6) return 0;

    /* ... and the world must have survived the depth test: if the middle of
     * the frame is still sky, every block was rejected.                    */
    unsigned char cp[4 * 64];
    glReadPixels(glrW / 2 - 4, glrH / 2 - 4, 8, 8, GL_RGBA, GL_UNSIGNED_BYTE, cp);
    int solid = 0;
    for (int i = 0; i < 64; i++) {
        int r = cp[i * 4 + 0], g = cp[i * 4 + 1], b = cp[i * 4 + 2];
        if (!(b > r + 10 && b > 40)) solid++;
    }
    return solid >= 1;
}

/* Draws one frame into easygl's canvas texture.  Returns 0 if it did not
 * happen, in which case the caller must fall back to the CPU path.       */
static int glr_frame(const M4* mvp) {
    if (!glrReady) return 0;
    if (!getcanvastex()) return 0;
    if (!glr_targets(getcanvaswidth(), getcanvasheight())) return 0;

    GLint of = 0, op = 0, ova = 0, ovb = 0, ovp[4] = { 0, 0, 0, 0 };
    GLint ot0 = 0, ot1 = 0, odm = 0;
    GLboolean od = 0, os = 0, obl = 0, ocl = 0, ocu = 0;
    float occ[4] = { 0, 0, 0, 0 };
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &of);
    glGetIntegerv(GL_CURRENT_PROGRAM, &op);
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &ova);
    glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &ovb);
    glGetIntegerv(GL_VIEWPORT, ovp);
    glGetBooleanv(GL_DEPTH_TEST, &od);
    glGetBooleanv(GL_SCISSOR_TEST, &os);
    glGetBooleanv(GL_BLEND, &obl);
    glGetBooleanv(GL_COLOR_LOGIC_OP, &ocl);
    glGetBooleanv(GL_CULL_FACE, &ocu);
    glGetIntegerv(GL_DEPTH_WRITEMASK, &odm);
    glGetFloatv(GL_COLOR_CLEAR_VALUE, occ);
    glActiveTexture(GL_TEXTURE0);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &ot0);
    glActiveTexture(GL_TEXTURE1);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &ot1);

    glBindFramebuffer(GL_FRAMEBUFFER, glrFbo);
    glViewport(0, 0, glrW, glrH);
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_BLEND);
    glDisable(GL_COLOR_LOGIC_OP);
    glDisable(GL_CULL_FACE);
    glClearColor(0.f, 0.f, 0.f, 1.f);
    glClearDepth(1.0);
    glDepthMask(GL_TRUE);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    /* The sky covers every pixel, but it must not write depth.  Its clip z
     * is 0, i.e. window depth 0.5, and real geometry sits near 0.995, so a
     * depth writing sky pass would make GL_LESS reject the entire world. */
    glUseProgram(glrSkyProg);
    glrBindVAO(glrSkyVao);
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glDrawArrays(GL_TRIANGLES, 0, 6);

    glUseProgram(glrProg);
    glrBindVAO(glrVao);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glDepthMask(GL_TRUE);
    glUniformMatrix4fv(glr_uMvp, 1, GL_FALSE, mvp->m);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, glrAlb);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, glrDec);
    glDrawArrays(GL_TRIANGLES, 0, glrVertN);

    /* Prove the path really produced a picture before it owns the canvas for
     * good.  One readback on the first frame; if it fails we drop back to
     * the CPU rasteriser instead of leaving a blank window on screen.     */
    int ok = 1;
    if (!glrChecked) {
        glrChecked = 1;
        ok = glr_verify();
        if (!ok) {
            fprintf(stderr, "glr: self check failed, using the CPU rasteriser\n");
            glrReady = 0;
        }
    }

    if (ok) {
        /* GPU side copy into the canvas texture; easygl's present then
         * blits it with no upload at all.                                 */
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, getcanvastex());
        glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, glrW, glrH);
    }

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, (GLuint)ot1);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, (GLuint)ot0);
    glrBindVAO((GLuint)ova);
    glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)of);
    glUseProgram((GLuint)op);
    glBindBuffer(GL_ARRAY_BUFFER, (GLuint)ovb);
    glViewport(ovp[0], ovp[1], ovp[2], ovp[3]);
    glClearColor(occ[0], occ[1], occ[2], occ[3]);
    glDepthMask(odm ? GL_TRUE : GL_FALSE);
    if (od)  glEnable(GL_DEPTH_TEST);    else glDisable(GL_DEPTH_TEST);
    if (os)  glEnable(GL_SCISSOR_TEST);  else glDisable(GL_SCISSOR_TEST);
    if (obl) glEnable(GL_BLEND);         else glDisable(GL_BLEND);
    if (ocl) glEnable(GL_COLOR_LOGIC_OP); else glDisable(GL_COLOR_LOGIC_OP);
    if (ocu) glEnable(GL_CULL_FACE);     else glDisable(GL_CULL_FACE);
    return ok;
}

#else  /* RR_HEADLESS */

static void glr_init(void) { }
static int  glr_frame(const M4* mvp) { (void)mvp; return 0; }

#endif /* !RR_HEADLESS */

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
        unsigned char b[3] = { (unsigned char)((c >> 16) & 255),
                               (unsigned char)((c >> 8) & 255),
                               (unsigned char)(c & 255) };
        fwrite(b, 1, 3, f);
    }
    fclose(f);
}

/* Number of chunks that actually hold geometry.  This was a C++ lambda. */
static int count_chunks(void) {
    int n = 0;
    for (int i = 0; i < NCHUNK; i++) if (g_chCount[i]) n++;
    return n;
}

int main(void) {
    fb_init(RR_W, RR_H);
    tex_init();
    world_gen();
#ifndef RR_HEADLESS
    /* The CPU fallback writes every pixel, so the read back that
     * GetImageBuffer() would do is pure waste. */
    setimagebuffermode(GX_IMGBUF_DISCARD);
#endif
    mesh_build();

    g_cam.pos = v3(24.5f, 6.0f, 24.5f);
    g_cam.vel = v3(0, 0, 0);
    g_cam.yaw = 0.6f; g_cam.pitch = -0.18f;
    g_cam.onGround = 0; g_cam.fly = 0; g_cam.sneak = 0;

    printf("MESH faces=%d  chunks=%d (non-empty=%d)\n", g_meshN, (int)NCHUNK,
           count_chunks());

    /* every heading must be present in the baked mesh */
    {
        int cnt[8]; memset(cnt, 0, sizeof(cnt));
        for (int i = 0; i < g_meshN; i++) cnt[g_mesh[i].dir]++;
        printf("DIRS ");
        for (int d = 0; d < 8; d++) printf("%s=%d ", DIR_NAME[d], cnt[d]);
        printf("\n");
        int tc[4]; memset(tc, 0, sizeof(tc));
        for (int i = 0; i < g_meshN; i++) tc[g_mesh[i].type]++;
        printf("TYPES air=%d grass=%d dirt=%d rock=%d\n", tc[0], tc[1], tc[2], tc[3]);
    }

    /* ---- self test: arrow rotation ---- */
    {
        /* the arrow stencil points at +v; check the tip lands on the right
         * axis after each heading's rotation                              */
        const char* nm[4] = { "east(+X)", "south(+Z)", "west(-X)", "north(-Z)" };
        for (int d = 0; d < 4; d++) {
            float rot = -1.5707963f * d;
            float cs = cosf(rot), sn = sinf(rot);
            /* tip of the stencil in the top face's uv: +v means (u=0.5,v=1) */
            float ou = 0.5f - 0.5f, ov = 1.0f - 0.5f;
            float du = ou * cs - ov * sn + 0.5f;
            float dv = ou * sn + ov * cs + 0.5f;
            /* top face u runs along +Z, v along +X */
            float wx = dv - 0.5f, wz = du - 0.5f;
            printf("ARROW dir%d %-9s -> world dir x=%+.2f z=%+.2f\n", d, nm[d], wx, wz);
        }
        printf("ARROW expect: east x=+1 z=0, south x=0 z=+1, west x=-1 z=0, north x=0 z=-1\n");
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
            int r = aabb_in_frustum(pl, box[i][0], box[i][1], box[i][2],
                                        box[i][3], box[i][4], box[i][5]);
            printf("FRUSTUM %-14s -> %s\n", nm[i], r ? "kept" : "culled");
        }
        printf("FRUSTUM expect: kept, culled, culled, culled, culled, culled\n");
    }

    /* ---- self test: a triangle that must cover the screen centre ---- */
    {
        fb_sky(); fb_clear_depth();
        CVtx a = { -0.5f, -0.5f, 0.0f, 1.0f, 0, 0, 0, 0, 1.0f };
        CVtx b = {  0.5f, -0.5f, 0.0f, 1.0f, 1, 0, 1, 0, 1.0f };
        CVtx c = {  0.0f,  0.5f, 0.0f, 1.0f, 0, 1, 0, 1, 1.0f };
        int before = g_pix;
        raster_tri(a, b, c, 1.0f);
        printf("SELFTEST centre triangle: pix=%d (expect ~115200)\n", g_pix - before);
        g_pix = before;
    }

    /* ---- self test: the matrix chain against hand computed values ---- */
    {
        M4 P = m4persp(1.2217f, 16.0f / 9.0f, RR_NEAR, RR_FAR);
        M4 V = m4view(v3(0, 0, 0), v3(0, 0, -1), v3(0, 1, 0));
        M4 M = m4mul(&P, &V);
        float pts[3][3] = { { 0, 0, -10 }, { 3, 0, -10 }, { 0, 0, 10 } };
        const char* nm[3] = { "front 10m", "right 3m/front 10m", "behind 10m" };
        for (int i = 0; i < 3; i++) {
            float x = pts[i][0], y = pts[i][1], z = pts[i][2];
            float cx = M.m[0]*x + M.m[1]*y + M.m[2]*z + M.m[3];
            float cy = M.m[4]*x + M.m[5]*y + M.m[6]*z + M.m[7];
            float cz = M.m[8]*x + M.m[9]*y + M.m[10]*z + M.m[11];
            float cw = M.m[12]*x + M.m[13]*y + M.m[14]*z + M.m[15];
            printf("MATTEST %-20s w=%8.3f  ndc=(%8.4f,%8.4f,%8.4f)\n",
                   nm[i], cw, cx / cw, cy / cw, cz / cw);
        }
        printf("MATTEST expect: front w=10 ndc=(0,0,0.9905); right ndc.x>0; behind w=-10\n");
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
        g_chunkIn = g_chunkOut = g_farout = g_backface = 0;
        clock_t t0 = clock();
        render_frame();
        fb_crosshair();
        double ms = 1000.0 * (double)(clock() - t0) / CLOCKS_PER_SEC;
        printf("frame %s: %.2f ms  chunkIn=%d chunkOut=%d farout=%d backface=%d faces=%d\n",
               poses[i].name, ms, g_chunkIn, g_chunkOut, g_farout, g_backface, g_faces);
        char path[128];
        snprintf(path, sizeof(path), "/data/workspace/out_%s.ppm", poses[i].name);
        ppm_write(path, g_fb.color, g_fb.w, g_fb.h);
        printf("wrote %s  raster=%d bboxbad=%d outside=%d zrange(low=%d high=%d) z=[%.4f..%.4f] pix=%d\n",
               path, g_raster, g_bboxbad, g_outside, g_zlow, g_zhigh, g_zmin, g_zmax, g_pix);
        g_bboxbad = g_outside = g_iwbad = g_zrange = g_zlow = g_zhigh = 0; g_zmin = 1e30f; g_zmax = -1e30f;
        g_faces = g_raster = g_culled = g_clipped = g_zfail = g_pix = 0;
    }

    fb_free();
    mesh_free();
    return 0;
}

#else

/* The shader path has no CPU buffer to poke, so its crosshair goes down
 * through normal easygl drawing after the frame has been copied.          */
static void crosshair_gl(void) {
    int cx = g_fb.w / 2, cy = g_fb.h / 2, L = 10;
    setlinecolor(RGB(0, 0, 0));
    line(cx - L - 1, cy, cx + L + 1, cy);
    line(cx, cy - L - 1, cx, cy + L + 1);
    setlinecolor(RGB(255, 255, 255));
    line(cx - L, cy, cx + L, cy);
    line(cx, cy - L, cx, cy + L);
}

int main(void) {
    initgraph(RR_W, RR_H);
    setbkcolor(RGB(20, 20, 40));
    setvsync(1);
    BeginBatchDraw();
    /* The CPU rasteriser overwrites every pixel, so the read back that
     * GetImageBuffer() does by default is a full canvas glReadPixels per
     * frame for data that is thrown away.  Skip it.                      */
    setimagebuffermode(GX_IMGBUF_DISCARD);

    /* easygl sizes its canvas in device pixels, which is not necessarily
     * RR_W x RR_H (DPI scaling).  Blitting a frame buffer of the wrong size
     * into it is one of the ways the window ends up blank, so take the real
     * numbers from the renderer instead of assuming.                      */
    fb_init(getcanvaswidth(), getcanvasheight());
    tex_init();
    world_gen();
    mesh_build();
    glr_init();                 /* after initgraph: needs the GL context   */
    gpumesh_build();            /* after initgraph: needs the GL context   */

    g_cam.pos = v3(24.5f, 6.0f, 24.5f);
    g_cam.vel = v3(0, 0, 0);
    g_cam.yaw = 0.6f; g_cam.pitch = -0.18f;
    g_cam.onGround = 0; g_cam.fly = 0; g_cam.sneak = 0;

    /* ---- pointer lock: hide the cursor, keep it inside the client area
     * and re-centre it every frame.  Re-centring produces a MOUSEMOVE whose
     * coordinates are the centre, and because lastX/lastY are set to the
     * centre as well that synthetic event contributes a delta of zero.    */
    HWND hwnd = GetHWnd();
    ShowCursor(FALSE);
    RECT crc;
    GetClientRect(hwnd, &crc);
    int ccx = (crc.left + crc.right) / 2;
    int ccy = (crc.top + crc.bottom) / 2;

    int fwd = 0, back = 0, left = 0, right = 0, jump = 0, sprint = 0, sneak = 0;
    int lastX = ccx, lastY = ccy, haveMouse = 0;
    double last = 0;
    int have_last = 0;
    float fps = 60.0f;
    char blk[64] = "air";

    while (1) {
        /* re-clip every frame so a moved or resized window stays locked */
        RECT rc;
        GetClientRect(hwnd, &rc);
        POINT tl = { rc.left, rc.top }, br = { rc.right, rc.bottom };
        ClientToScreen(hwnd, &tl);
        ClientToScreen(hwnd, &br);
        RECT clip = { tl.x, tl.y, br.x, br.y };
        ClipCursor(&clip);
        ccx = (rc.left + rc.right) / 2;
        ccy = (rc.top + rc.bottom) / 2;

        /* input */
        int mdx = 0, mdy = 0, gotMouse = 0;
        ExMessage m;
        while (peekmessage(&m, EX_MOUSE | EX_KEY)) {
            if (m.message == WM_MOUSEMOVE) {
                if (haveMouse) { mdx += m.x - lastX; mdy += m.y - lastY; gotMouse = 1; }
                lastX = m.x; lastY = m.y; haveMouse = 1;
            } else if (m.message == WM_KEYDOWN) {
                switch (m.vkcode) {
                    case 'W': fwd = 1; break;
                    case 'S': back = 1; break;
                    case 'A': left = 1; break;
                    case 'D': right = 1; break;
                    case RRK_SPACE: jump = 1; break;
                    case RRK_SHIFT: sneak = 1; break;
                    case RRK_CTRL:  sprint = 1; break;
                    case 'F': g_cam.fly = !g_cam.fly; break;
                    case RRK_F9: g_forceCpu = !g_forceCpu; break;
                    case RRK_ESC: goto done;
                }
            } else if (m.message == WM_KEYUP) {
                switch (m.vkcode) {
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
        if (gotMouse) cam_look(&g_cam, (float)mdx, (float)mdy);

        /* timing */
        double now = (double)clock() / CLOCKS_PER_SEC;
        float dt = have_last ? (float)(now - last) : 1.0f / 60.0f;
        have_last = 1; last = now;
        if (dt > 0.1f) dt = 0.1f;
        if (dt < 0.0005f) dt = 0.0005f;

        /* the canvas can change size under us (window resize / DPI) */
        if (getcanvaswidth() != g_fb.w || getcanvasheight() != g_fb.h)
            fb_init(getcanvaswidth(), getcanvasheight());

        cam_step(&g_cam, dt, fwd, back, left, right, jump, sprint, sneak);
        int usedGpu = render_frame();
        /* The crosshair lives in the CPU buffer, so the shader path has to
         * draw its own straight onto the canvas instead.                  */
        if (usedGpu) crosshair_gl(); else fb_crosshair();

        /* smoothed frame rate and the block being stood on, in the title */
        if (dt > 0.0f) fps += (1.0f / dt - fps) * 0.08f;
        block_under_camera(blk, sizeof(blk));
        {
            char title[160];
            snprintf(title, sizeof(title), "voxelgl  [%.0f fps]  [%s]  standing on: %s",
                     fps, usedGpu ? "GPU" : "CPU", blk);
            SetWindowTextA(hwnd, title);
        }

        /* Blit only when the CPU rasteriser drew.  GetImageBuffer() is a
         * full canvas readback and writing it over a GPU frame would blank
         * the window, so the shader path skips both.                      */
        if (!usedGpu) {
            DWORD* buf = GetImageBuffer();
            if (buf) present(g_fb.color, (rr_u32*)buf, g_fb.w * g_fb.h);
        }
        FlushBatchDraw();

        /* put the pointer back in the middle for the next frame */
        POINT sc = { ccx, ccy };
        ClientToScreen(hwnd, &sc);
        SetCursorPos(sc.x, sc.y);
        lastX = ccx; lastY = ccy;
    }

done:
    ShowCursor(TRUE);
    ClipCursor(NULL);
    fb_free();
    mesh_free();
    closegraph();
    return 0;
}

#endif

