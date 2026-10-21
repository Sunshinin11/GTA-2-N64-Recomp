/* Renderer N64 (etapa 3): desenha direto no RDP, via rdpq, a lista de comandos que o motor gta2_re gera.
 *
 * O motor ja projeta tudo para o espaco de tela (x,y em 640x480), entao nao ha 3D aqui: so triangulos 2D
 * com cor Gouraud + textura. As texturas do GTA 2 sao 8 bits com paleta (paginas de 256 px); aqui cada textura
 * e convertida SOB DEMANDA para RGBA16, reduzida com MEDIA POR AREA (nao por amostragem pontual) para ate 2048
 * texels (= os 4 KB inteiros da TMEM), e guardada num cache simples. A cor 0 e' transparente; as bordas
 * transparentes herdam a cor dos vizinhos para o filtro bilinear nao escurecer contornos.
 *
 * "Laboratorio" de render (botoes do controle, edge-triggered):
 *   L        liga/desliga filtro bilinear
 *   R        perspectiva: 0 desligada, 1 ligada (w do motor). O antigo modo 2 (s,t * 1/w) quebrava pecas 3D e saiu.
 *   C-cima   depuracao: 0 normal, 1 esconde translucidos, 2 so translucidos, 3 cor chapada por textura
 *
 * Indicadores: barra de RAM no topo, quadradinho piscando (loop vivo), botoes do joypad (embaixo a esquerda),
 * estado das opcoes (embaixo a direita: filtro, perspectiva, depuracao).
 */
#include <libdragon.h>
#include <malloc.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "render/n64tex.h"

typedef struct { float x, y, w, u, v; uint32_t rgba; } FVert;                              /* = FrameVertex (render/frame.h) */
typedef struct { uint32_t kind, texture, blend, first, count, colour; int32_t rect[4]; } FCmd; /* = FrameCommand */

#define CACHE_N     160           /* captura no desenho: copias do mesmo quadro precisam coexistir */
#define SLOT_U16    2048          /* 4096 bytes por entrada = TMEM inteira em RGBA16 */
#define MAX_TEXELS  2048

typedef struct { const N64Tex *owner; uint16_t gen, tw, th, pw; uint32_t used; float req; } CEnt; /* used = g_frames+1 do ultimo uso (0 = nunca); req = escala pedida na conversao */
#define SCALE_MIN   0.25f  /* menor reducao aceita por escala de tela */
#define SCALE_UP    1.25f  /* reconverte (maior) se a textura passar a aparecer 25% maior na tela */
#define CACHE_GUARD 3  /* quadros que um slot fica protegido apos o uso: o RDP le a textura depois, de forma assincrona */
static CEnt g_c[CACHE_N];
static uint16_t *g_cbuf;
static int g_next;
static int g_active;
static uint32_t g_frames;
static uint32_t g_conv_frame;                   /* conversoes neste frame (diagnostico) */
static uint32_t g_tx_r, g_tx_g, g_tx_b, g_tx_n; /* soma dos texels opacos convertidos (diagnostico) */

/* opcoes do laboratorio */
static int g_opt_filter = 1;
static int g_opt_persp = 0;
static int g_dbg = 0;

/* estatisticas */
static uint32_t g_tris, g_tris_skip;
static uint32_t g_wvar;                         /* triangulos com w variando >2% entre vertices */
static uint64_t g_t_wait, g_t_draw, g_t_conv;   /* ticks acumulados */
static uint32_t g_last_present_ticks;
static uint64_t g_t_period;
/* audio (audio_n64.c) */
extern void gta2_n64_audio_init(void);
extern void gta2_n64_audio_poll(void);

int gta2_n64_gfx_active(void) { return g_active; }

void gta2_n64_render_toggle(int which)
{
    if (which == 0) g_opt_filter = !g_opt_filter;
    else if (which == 1) g_opt_persp = (g_opt_persp + 1) % 2;
    else if (which == 2) g_dbg = (g_dbg + 1) % 4;
    fprintf(stderr, "[opt] filtro=%d persp=%d dbg=%d\n", g_opt_filter, g_opt_persp, g_dbg);
}

void gta2_n64_tex_forget(const N64Tex *t)
{
    if (t && t->slot >= 0 && t->slot < CACHE_N && g_c[t->slot].owner == t) g_c[t->slot].owner = 0;
}

/* devolve a entrada do cache para a textura (convertendo se preciso); -1 se nao ha dados */
/* scale = pixels de tela por texel (<= 1): a textura e' reduzida com media por area para o tamanho em que aparece */
/* Leitura da ROM (pixels e paletas mapeados na cartucha): a CPU so le a ROM direito em palavras de 32 bits
 * alinhadas, e nunca durante um DMA do PI. Byte solto volta da posicao errada da palavra (sprites quebrados).
 * io_read() da libdragon espera o PI ficar livre e le 32 bits; daqui os bytes sao extraidos na posicao certa. */
static inline int in_rom(const void *p) { const uintptr_t a = (uintptr_t)p; return a >= 0xB0000000u && a < 0xC0000000u; }
static uint8_t g_srcbuf[256 * 256]; /* copia em RAM das linhas de origem (stride 256, como as paginas do .sty) */
/* copia len bytes de src (ROM ou RAM) para dst; na ROM le so palavras de 32 bits alinhadas, via io_read */
void gta2_n64_rom_copy(void *dst, const void *src, unsigned len)
{
    if (!in_rom(src)) { memcpy(dst, src, len); return; }
    uint8_t *o = (uint8_t *)dst;
    const uint32_t pi = (uint32_t)((uintptr_t)src & 0x1FFFFFFFu);
    uint32_t p = pi & ~3u;
    unsigned k = 0;
    int j = (int)(pi & 3u);
    while (k < len) {
        const uint32_t v = io_read(p);
        for (; j < 4 && k < len; ++j) o[k++] = (uint8_t)(v >> (24 - 8 * j));
        j = 0;
        p += 4;
    }
}

static const uint8_t *src_pixels(const N64Tex *t, int w, int h)
{
    if (!in_rom(t->pix)) return t->pix;
    if (w > 256) w = 256;
    if (h > 256) h = 256;
    for (int y = 0; y < h; ++y) {
        const uint32_t pi = (uint32_t)(((uintptr_t)t->pix + (uintptr_t)y * 256u) & 0x1FFFFFFFu);
        uint8_t *o = g_srcbuf + y * 256;
        uint32_t p = pi & ~3u;
        int k = 0, j = (int)(pi & 3u);
        while (k < w) {
            const uint32_t v = io_read(p);
            for (; j < 4 && k < w; ++j) o[k++] = (uint8_t)(v >> (24 - 8 * j));
            j = 0;
            p += 4;
        }
    }
    return g_srcbuf;
}

static int tex_slot(N64Tex *t, float scale)
{
    if (!t || !t->pix || !t->pal || t->w == 0 || t->h == 0) return -1;
    if (t->slot >= 0 && g_c[t->slot].owner == t && g_c[t->slot].gen == t->gen) {
        if (scale <= g_c[t->slot].req * SCALE_UP) { g_c[t->slot].used = g_frames + 1; return t->slot; }
        g_c[t->slot].owner = 0; /* aparece maior que na conversao: reconverte com mais resolucao (so cresce) */
    }
    if (!g_cbuf) {
        g_cbuf = (uint16_t *)memalign(64, (size_t)CACHE_N * SLOT_U16 * 2);
        if (!g_cbuf) return -1;
        memset(g_c, 0, sizeof(g_c));
    }
    const uint32_t t0 = timer_ticks();
    const int w = t->w, h = t->h;
    int tw = w, th = h;
    if (scale < 0.95f) { /* reduz para o tamanho na tela (sprites: ~metade; blocos: ~44 de 64) */
        tw = (int)((float)w * scale + 0.5f);
        th = (int)((float)h * scale + 0.5f);
        if (tw < 1) tw = 1;
        if (th < 1) th = 1;
    }
    if (tw * th > MAX_TEXELS) { /* limite da TMEM (4 KB em RGBA16) */
        const float f = sqrtf((float)MAX_TEXELS / (float)(tw * th));
        tw = (int)((float)tw * f) & ~3;
        if (tw < 4) tw = 4;
        th = (int)((float)th * f + 0.5f);
        if (th < 1) th = 1;
    }
    if (tw > w) tw = w;
    if (th > h) th = h;
    if (tw > 256) tw = 256; /* limite do vetor xs[] (paginas do .sty tem 256 px) */
    int pw = (tw + 3) & ~3;
    while (pw * th > MAX_TEXELS && th > 1) --th;

    /* escolhe um slot que o RDP nao vai mais ler: nunca usado ou sem uso nos ultimos CACHE_GUARD quadros */
    int slot = -1;
    for (int i = 0; i < CACHE_N; ++i) {
        const int s = (g_next + i) % CACHE_N;
        if (g_c[s].used == 0 || (g_frames + 1) - g_c[s].used >= CACHE_GUARD) { slot = s; break; }
    }
    if (slot < 0) { /* todos em uso recente: usa um que nao foi capturado NESTE quadro, depois de esperar o RDP */
        for (int i = 0; i < CACHE_N; ++i) {
            const int s = (g_next + i) % CACHE_N;
            if (g_c[s].used != g_frames + 1) { slot = s; break; }
        }
        if (slot < 0) return -1; /* cache cheio so com texturas deste quadro: desenha sem textura */
        rspq_wait();
    }
    g_next = (slot + 1) % CACHE_N;
    uint16_t *dst = g_cbuf + (size_t)slot * SLOT_U16;

    uint32_t prgb[256];
    const uint32_t *pal = t->pal;
    if (in_rom(pal)) {
        for (int i = 0; i < 256; ++i) prgb[i] = io_read((uint32_t)((uintptr_t)&pal[i * 64] & 0x1FFFFFFFu));
    } else {
        for (int i = 0; i < 256; ++i) prgb[i] = pal[i * 64];
    }
    const uint8_t *src = src_pixels(t, w, h);
    uint16_t xs[260];
    for (int x = 0; x <= tw; ++x) xs[x] = (uint16_t)(x * w / tw);

    int any_clear = 0;
    for (int y = 0; y < th; ++y) {
        const int y0 = y * h / th;
        int y1 = (y + 1) * h / th;
        if (y1 <= y0) y1 = y0 + 1;
        uint16_t *o = dst + y * pw;
        for (int x = 0; x < tw; ++x) {
            const int x0 = xs[x];
            int x1 = xs[x + 1];
            if (x1 <= x0) x1 = x0 + 1;
            unsigned sr = 0, sg = 0, sb = 0, n = 0;
            for (int yy = y0; yy < y1; ++yy) {
                const uint8_t *row = src + (size_t)yy * 256;
                for (int xx = x0; xx < x1; ++xx) {
                    const uint8_t idx = row[xx];
                    if (idx) {
                        const uint32_t c = prgb[idx];
                        sr += (c >> 16) & 255; sg += (c >> 8) & 255; sb += c & 255; ++n;
                    }
                }
            }
            const unsigned area = (unsigned)((x1 - x0) * (y1 - y0));
            if (n && n * 3 >= area) {
                const unsigned r = sr / n, g = sg / n, b = sb / n;
                o[x] = (uint16_t)(((r >> 3) << 11) | ((g >> 3) << 6) | ((b >> 3) << 1) | 1);
                g_tx_r += r; g_tx_g += g; g_tx_b += b; ++g_tx_n;
            } else {
                o[x] = 0;
                any_clear = 1;
            }
        }
        for (int x = tw; x < pw; ++x) o[x] = 0;
    }
    /* texels transparentes herdam a cor de um vizinho opaco (alfa continua 0): o bilinear nao escurece as bordas */
    if (any_clear) {
        for (int y = 0; y < th; ++y) {
            uint16_t *o = dst + y * pw;
            for (int x = 0; x < tw; ++x) {
                if (o[x] & 1) continue;
                uint16_t nb = 0;
                if (x > 0 && (o[x - 1] & 1)) nb = o[x - 1];
                else if (x + 1 < tw && (o[x + 1] & 1)) nb = o[x + 1];
                else if (y > 0 && (dst[(y - 1) * pw + x] & 1)) nb = dst[(y - 1) * pw + x];
                else if (y + 1 < th && (dst[(y + 1) * pw + x] & 1)) nb = dst[(y + 1) * pw + x];
                if (nb) o[x] = (uint16_t)(nb & ~1);
            }
        }
    }
    data_cache_hit_writeback(dst, (unsigned long)pw * th * 2);
    g_c[slot].owner = t; g_c[slot].gen = t->gen; g_c[slot].tw = (uint16_t)tw; g_c[slot].th = (uint16_t)th; g_c[slot].pw = (uint16_t)pw;
    g_c[slot].used = g_frames + 1;
    g_c[slot].req = scale;
    t->slot = (int16_t)slot;
    ++g_conv_frame;
    g_t_conv += (uint32_t)(timer_ticks() - t0);
    return slot;
}

static float SCR_W = 320.0f, SCR_H = 240.0f; /* ajustado em present() pelo framebuffer real (libdragon: 640x240) */

static const rdpq_trifmt_t kFmtTex = { .pos_offset = 0, .shade_offset = 2, .tex_offset = 6, .z_offset = -1 };

/* escala do comando: maior razao (pixels de tela / texels) entre os triangulos, limitada a [SCALE_MIN, 1] */
static float cmd_scale(const FCmd *cm, const FVert *verts, const N64Tex *t, float sx, float sy)
{
    if (!t || t->w == 0 || t->h == 0) return 1.0f;
    const float texels = (float)t->w * (float)t->h;
    float best = 0.0f;
    for (uint32_t i = 0; i + 2 < cm->count; i += 3) {
        const FVert *p = &verts[cm->first + i];
        const float ax = (p[1].x - p[0].x) * sx, ay = (p[1].y - p[0].y) * sy;
        const float bx = (p[2].x - p[0].x) * sx, by = (p[2].y - p[0].y) * sy;
        const float a_scr = fabsf(ax * by - bx * ay);
        const float a_tex = fabsf((p[1].u - p[0].u) * (p[2].v - p[0].v) - (p[2].u - p[0].u) * (p[1].v - p[0].v)) * texels;
        if (a_scr < 1.0f || a_tex < 1.0f || !(a_scr < 4.0e6f)) continue;
        const float r = sqrtf(a_scr / a_tex);
        if (r > best) best = r;
    }
    if (best <= 0.0f) return 1.0f;
    if (best > 1.0f) best = 1.0f;
    if (best < SCALE_MIN) best = SCALE_MIN;
    return best;
}

/* Captura no momento do desenho (chamada por gbh_n64.cpp/Draw): converte a textura AGORA, antes que o motor
 * reaproveite pixels/paleta/tamanho para outro sprite no mesmo quadro, e devolve um token do slot (0 = sem textura).
 * a_scr: area do triangulo na tela de referencia (640x480); a_tex: area em texels. */
#define TEX_TOKEN_BASE 0x10000u
static float g_last_sx = 1.0f, g_last_sy = 0.5f; /* escala tela-referencia -> framebuffer do ultimo present */
uint32_t gta2_n64_tex_capture(N64Tex *t, float a_scr, float a_tex)
{
    if (!t) return 0;
    float scale = 1.0f;
    if (a_scr > 0.0f && a_tex >= 1.0f) {
        scale = sqrtf(a_scr * g_last_sx * g_last_sy / a_tex);
        if (scale > 1.0f) scale = 1.0f;
        if (scale < SCALE_MIN) scale = SCALE_MIN;
    }
    const int slot = tex_slot(t, scale);
    return slot < 0 ? 0u : TEX_TOKEN_BASE + (uint32_t)slot;
}

static inline double ticks_ms(uint64_t t) { return (double)t * 1000.0 / (double)TICKS_PER_SECOND; }

void gta2_n64_present(const void *cmds_, int cmdCount, const void *verts_, int vertCount, int refWidth)
{
    static int inited;
    const uint32_t t_start = timer_ticks();
    if (!inited) {
        rdpq_init();
        gta2_n64_audio_init(); /* depois do rdpq_init: o mixer usa a fila do RSP */
        inited = 1;
        g_active = 1;
    }
    if (g_last_present_ticks) g_t_period += (uint32_t)(t_start - g_last_present_ticks);
    g_last_present_ticks = t_start;

    const FCmd *cmds = (const FCmd *)cmds_;
    const FVert *verts = (const FVert *)verts_;
    if (refWidth <= 0) refWidth = 640;
    g_conv_frame = 0;

    gta2_n64_audio_poll();
    surface_t *disp = display_get();
    g_t_wait += (uint32_t)(timer_ticks() - t_start); /* tempo esperando um buffer livre (vsync) */
    SCR_W = (float)disp->width;
    SCR_H = (float)disp->height;
    const int IW = (int)disp->width, IH = (int)disp->height;
    const float sx = SCR_W / (float)refWidth, sy = SCR_H / 480.0f;
    g_last_sx = sx; g_last_sy = sy;
    rdpq_attach(disp, NULL);
    rdpq_set_mode_fill(RGBA32(0, 0, 0, 255));
    rdpq_fill_rectangle(0, 0, IW, IH);

    for (int c = 0; c < cmdCount; ++c) {
        const FCmd *cm = &cmds[c];
        if (cm->kind == 1) { /* limpar retangulo */
            int x0 = (int)(cm->rect[0] * sx), y0 = (int)(cm->rect[1] * sy);
            int x1 = (int)(cm->rect[2] * sx), y1 = (int)(cm->rect[3] * sy);
            if (x0 < 0) x0 = 0;
            if (y0 < 0) y0 = 0;
            if (x1 > IW) x1 = IW;
            if (y1 > IH) y1 = IH;
            if (x1 > x0 && y1 > y0) {
                rdpq_set_mode_fill(RGBA32(cm->colour & 0xFF, (cm->colour >> 8) & 0xFF, (cm->colour >> 16) & 0xFF, 255));
                rdpq_fill_rectangle(x0, y0, x1, y1);
            }
            continue;
        }
        if (cm->kind != 0 || cm->count < 3) continue;
        if (g_dbg == 1 && cm->blend != 0) continue;
        if (g_dbg == 2 && cm->blend == 0) continue;

        int slot;
        if (cm->texture >= TEX_TOKEN_BASE && cm->texture < TEX_TOKEN_BASE + CACHE_N) { /* capturada no desenho */
            slot = (g_dbg == 3) ? -1 : (int)(cm->texture - TEX_TOKEN_BASE);
            if (slot >= 0) g_c[slot].used = g_frames + 1;
        } else {
            N64Tex *tx = (N64Tex *)(uintptr_t)cm->texture;
            slot = (g_dbg == 3) ? -1 : tex_slot(tx, cmd_scale(cm, verts, tx, sx, sy));
        }
        if (slot < 0 && cm->blend != 0 && g_dbg != 3) continue; /* sem dados e translucido: pula */

        rdpq_set_mode_standard();
        rdpq_mode_persp(g_opt_persp != 0);
        rdpq_mode_filter(g_opt_filter ? FILTER_BILINEAR : FILTER_POINT);
        float flat_r = 1.0f, flat_g = 1.0f, flat_b = 1.0f;
        if (slot >= 0) {
            const CEnt *e = &g_c[slot];
            surface_t surf = surface_make(g_cbuf + (size_t)slot * SLOT_U16, FMT_RGBA16, e->pw, e->th, e->pw * 2);
            rdpq_mode_combiner(RDPQ_COMBINER_TEX_SHADE);
            rdpq_mode_alphacompare(cm->blend == 0 ? 128 : 0);
            rdpq_tex_upload(TILE0, &surf, NULL);
        } else {
            rdpq_mode_combiner(RDPQ_COMBINER_SHADE);
            rdpq_mode_alphacompare(0);
            if (g_dbg == 3) { /* cor chapada pelo ponteiro da textura */
                const uint32_t hsh = (cm->texture >> 3) * 2654435761u;
                flat_r = 0.25f + (float)((hsh >> 8) & 0xFF) / 340.0f;
                flat_g = 0.25f + (float)((hsh >> 16) & 0xFF) / 340.0f;
                flat_b = 0.25f + (float)((hsh >> 24) & 0xFF) / 340.0f;
            }
        }
        if (cm->blend == 1)      rdpq_mode_blender(RDPQ_BLENDER((IN_RGB, IN_ALPHA, MEMORY_RGB, INV_MUX_ALPHA)));
        else if (cm->blend == 2) rdpq_mode_blender(RDPQ_BLENDER((IN_RGB, IN_ALPHA, MEMORY_RGB, ONE)));
        else                     rdpq_mode_blender(0);

        const float ts = slot >= 0 ? (float)g_c[slot].tw : 1.0f, tt = slot >= 0 ? (float)g_c[slot].th : 1.0f;
        for (uint32_t t = 0; t + 2 < cm->count; t += 3) {
            const FVert *p = &verts[cm->first + t];
            float v[3][9];
            int bad = 0;
            float minx = 1e9f, maxx = -1e9f, miny = 1e9f, maxy = -1e9f;
            float wmax = 0.0f, wmin = 1e30f;
            for (int k = 0; k < 3; ++k) {
                float x = p[k].x * sx, y = p[k].y * sy;
                if (!(x > -1500.0f && x < 1500.0f && y > -1500.0f && y < 1500.0f)) { bad = 1; break; }
                if (x < minx) minx = x;
                if (x > maxx) maxx = x;
                if (y < miny) miny = y;
                if (y > maxy) maxy = y;
                if (p[k].w > wmax) wmax = p[k].w;
                if (p[k].w < wmin) wmin = p[k].w;
            }
            if (bad || maxx < 0 || minx > SCR_W || maxy < 0 || miny > SCR_H) { ++g_tris_skip; continue; }
            const int persp_ok = g_opt_persp && wmin > 0.0f && wmax < 1e20f;
            if (wmin > 0.0f && wmax > wmin * 1.02f) ++g_wvar;
            for (int k = 0; k < 3; ++k) {
                const uint32_t d = p[k].rgba;
                v[k][0] = p[k].x * sx; v[k][1] = p[k].y * sy;
                v[k][2] = (float)(d & 0xFF) / 255.0f * flat_r;
                v[k][3] = (float)((d >> 8) & 0xFF) / 255.0f * flat_g;
                v[k][4] = (float)((d >> 16) & 0xFF) / 255.0f * flat_b;
                v[k][5] = (cm->blend == 0) ? 1.0f : (float)(d >> 24) / 255.0f;
                const float iw = persp_ok ? p[k].w / wmax : 1.0f;      /* normalizado: o que importa e a razao entre vertices */
                const float pm = (persp_ok && g_opt_persp == 2) ? iw : 1.0f;
                v[k][6] = p[k].u * ts * pm; v[k][7] = p[k].v * tt * pm; v[k][8] = iw;
            }
            ++g_tris;
            if (slot >= 0) rdpq_triangle(&kFmtTex, v[0], v[1], v[2]);
            else           rdpq_triangle(&TRIFMT_SHADE, v[0], v[1], v[2]);
        }
    }

    /* indicadores: barra de RAM (verde < 70%, amarela < 90%, vermelha acima), loop vivo, joypad, opcoes */
    {
        struct mallinfo mi = mallinfo();
        const float usable = 6100.0f * 1024.0f;
        float f = (float)mi.uordblks / usable;
        if (f > 1.0f) f = 1.0f;
        rdpq_set_mode_fill(f < 0.7f ? RGBA32(0, 200, 0, 255) : f < 0.9f ? RGBA32(230, 200, 0, 255) : RGBA32(230, 0, 0, 255));
        rdpq_fill_rectangle(0, 0, (int)(f * SCR_W) + 1, 3);
        rdpq_set_mode_fill((g_frames & 16) ? RGBA32(255, 255, 255, 255) : RGBA32(60, 60, 60, 255));
        rdpq_fill_rectangle(IW - 8, 4, IW, 8);
        {
            extern unsigned gta2_n64_pad_raw;
            for (int b = 0; b < 10; ++b) {
                rdpq_set_mode_fill((gta2_n64_pad_raw >> b) & 1 ? RGBA32(0, 255, 0, 255) : RGBA32(90, 0, 0, 255));
                rdpq_fill_rectangle(4 + b * 10, IH - 10, 4 + b * 10 + 8, IH - 3);
            }
        }
        {   /* opcoes: filtro (verde=bilinear), perspectiva (apagado/amarelo/azul), depuracao (cinza/laranja/roxo/ciano) */
            rdpq_set_mode_fill(g_opt_filter ? RGBA32(0, 255, 0, 255) : RGBA32(90, 0, 0, 255));
            rdpq_fill_rectangle(IW - 34, IH - 10, IW - 26, IH - 3);
            rdpq_set_mode_fill(g_opt_persp == 0 ? RGBA32(90, 0, 0, 255) : g_opt_persp == 1 ? RGBA32(255, 220, 0, 255) : RGBA32(60, 140, 255, 255));
            rdpq_fill_rectangle(IW - 22, IH - 10, IW - 14, IH - 3);
            rdpq_set_mode_fill(g_dbg == 0 ? RGBA32(70, 70, 70, 255) : g_dbg == 1 ? RGBA32(255, 140, 0, 255) : g_dbg == 2 ? RGBA32(170, 60, 255, 255) : RGBA32(0, 220, 220, 255));
            rdpq_fill_rectangle(IW - 10, IH - 10, IW - 2, IH - 3);
        }
    }

    rdpq_detach_show();
    ++g_frames;
    gta2_n64_audio_poll();
    g_t_draw += (uint32_t)(timer_ticks() - t_start);
    if ((g_frames % 120) == 0) {
        struct mallinfo mi = mallinfo();
        const double n = 120.0;
        const double period = ticks_ms(g_t_period) / n;       /* tempo medio entre dois frames apresentados */
        const double wait = ticks_ms(g_t_wait) / n;           /* esperando vsync / buffer */
        const double draw = (ticks_ms(g_t_draw) - ticks_ms(g_t_wait)) / n; /* montar a lista no RDP (inclui conversoes) */
        const double conv = ticks_ms(g_t_conv) / n;
        fprintf(stderr, "[perf] periodo=%.1fms (%.1f fps) espera=%.1fms desenho=%.1fms (conv_tex=%.2fms) jogo~=%.1fms tris=%u skip=%u wvar=%u cmds=%d verts=%d heap=%uKB conv=%lu\n",
                period, period > 0.0 ? 1000.0 / period : 0.0, wait, draw, conv, period - wait - draw,
                (unsigned)(g_tris / 120), (unsigned)(g_tris_skip / 120), (unsigned)(g_wvar / 120), cmdCount, vertCount,
                (unsigned)(mi.uordblks / 1024), (unsigned long)g_conv_frame);
        g_t_period = g_t_wait = g_t_draw = g_t_conv = 0;
        g_tris = g_tris_skip = g_wvar = 0;
        /* amostra: ate 3 triangulos com w variando, para ver se w e' 1/z (rhw) ou profundidade */
        {
            int shown = 0;
            for (int c = 0; c < cmdCount && shown < 3; ++c) {
                const FCmd *cm = &cmds[c];
                if (cm->kind != 0) continue;
                for (uint32_t t = 0; t + 2 < cm->count && shown < 3; t += 3) {
                    const FVert *p = &verts[cm->first + t];
                    float a = p[0].w, b = p[1].w, d = p[2].w;
                    float mx = a > b ? (a > d ? a : d) : (b > d ? b : d), mn = a < b ? (a < d ? a : d) : (b < d ? b : d);
                    if (mn > 0.0f && mx > mn * 1.3f) {
                        fprintf(stderr, "[w] tri: x=(%d,%d,%d) y=(%d,%d,%d) w=(%.4f,%.4f,%.4f) uv0=(%.2f,%.2f)\n",
                                (int)p[0].x, (int)p[1].x, (int)p[2].x, (int)p[0].y, (int)p[1].y, (int)p[2].y, a, b, d, p[0].u, p[0].v);
                        ++shown;
                    }
                }
            }
        }
    }
}
