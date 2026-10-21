// Ponto de entrada N64 do motor gta2_re (decomp do GTA 2). Etapa 1: boot com renderer nulo.
// Mostra no console: memoria antes/depois da inicializacao do motor, quadros e apresentacoes.
// O jogo (WinMain_5E53F0) roda por fatias: gta2_game_frame() = Gta2Port_RunLoopSlice().
#include <libdragon.h>
#include <malloc.h>
#include <unistd.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

int gta2_game_init(void);
int gta2_game_frame(void);
void gta2_game_key(int scancode, int vk, int down);
uint32_t gta2_game_presents(void);
void gta2_install_exception_fixups(void);
void gta2_exception_stats(uint32_t *unaligned, uint32_t *fpu);

// Relogio monotonico em ms para o motor (timeGetTime, QueryPerformanceCounter...).
double gta2_n64_now_ms(void)
{
    return (double)timer_ticks() * 1000.0 / (double)TICKS_PER_SECOND;
}

#ifndef N64_DEBUG_INPUT
#define N64_DEBUG_INPUT 0 /* 1 = loga cada botao/tecla ([pad]/[key]) */
#endif
typedef struct { int sc, vk; } KeyDef;
enum { K_UP, K_DOWN, K_LEFT, K_RIGHT, K_W, K_S, K_A, K_D, K_ENTER, K_ATTACK, K_JUMP, K_ESC, K_PREV, K_NEXT, K_RADIO, K_COUNT };
static const KeyDef kKeys[K_COUNT] = {
    {0xC8, 0x26}, {0xD0, 0x28}, {0xCB, 0x25}, {0xCD, 0x27}, // setas (menus)
    {0x11, 0x57}, {0x1F, 0x53}, {0x1E, 0x41}, {0x20, 0x44}, // W S A D (jogo)
    {0x1C, 0x0D}, {0x26, 0x4C}, {0x39, 0x20}, {0x01, 0x1B}, // Enter, ataque L, pulo/freio, Esc
    {0x24, 0x4A}, {0x25, 0x4B},                             // arma anterior J / proxima K
    {0x3B, 0x70},                                           // F1: proxima estacao de radio
};
static uint8_t gKeyDown[K_COUNT];
unsigned gta2_n64_pad_raw; /* botoes crus do joypad (bits), lido pelo render para o indicador na tela */

static void set_key(int k, int down)
{
    if (gKeyDown[k] != (uint8_t)down)
    {
        gKeyDown[k] = (uint8_t)down;
        if (N64_DEBUG_INPUT) printf("[key] sc=%02X %s\n", (unsigned)kKeys[k].sc, down ? "DOWN" : "up");
        gta2_game_key(kKeys[k].sc, kKeys[k].vk, down);
    }
}

static void poll_input(void)
{
    joypad_poll();
    joypad_inputs_t in = joypad_get_inputs(JOYPAD_PORT_1);
    {
        static int last_conn = -1;
        int conn = joypad_is_connected(JOYPAD_PORT_1);
        if (conn != last_conn) { printf("[pad] porta1 conectada=%d\n", conn); last_conn = conn; }
        static unsigned last_raw;
        gta2_n64_pad_raw = 0;
        unsigned raw = (in.btn.a) | (in.btn.b << 1) | (in.btn.z << 2) | (in.btn.start << 3) | (in.btn.d_up << 4) | (in.btn.d_down << 5) |
                       (in.btn.d_left << 6) | (in.btn.d_right << 7) | (in.btn.c_left << 8) | (in.btn.c_right << 9);
        gta2_n64_pad_raw = raw;
        if (raw != last_raw) { if (N64_DEBUG_INPUT) printf("[pad] botoes=%03X stick=%d,%d\n", raw, (int)in.stick_x, (int)in.stick_y); last_raw = raw; }
    }
    int up = in.btn.d_up || in.stick_y > 40, down = in.btn.d_down || in.stick_y < -40;
    int left = in.btn.d_left || in.stick_x < -40, right = in.btn.d_right || in.stick_x > 40;
    set_key(K_UP, up);   set_key(K_W, up);
    set_key(K_DOWN, down); set_key(K_S, down);
    set_key(K_LEFT, left); set_key(K_A, left);
    set_key(K_RIGHT, right); set_key(K_D, right);
    set_key(K_ENTER, in.btn.a);
    set_key(K_ATTACK, in.btn.b);
    set_key(K_JUMP, in.btn.z);
    set_key(K_ESC, in.btn.start);
    set_key(K_PREV, in.btn.c_left);
    set_key(K_NEXT, in.btn.c_right);
    set_key(K_RADIO, in.btn.c_down); /* C-baixo troca a radio */
    {   /* laboratorio de render: L = filtro, R = perspectiva, C-cima = depuracao (so na borda de subida) */
        extern void gta2_n64_render_toggle(int which);
        static int pl, pr, pc;
        if (in.btn.l && !pl) gta2_n64_render_toggle(0);
        if (in.btn.r && !pr) gta2_n64_render_toggle(1);
        if (in.btn.c_up && !pc) gta2_n64_render_toggle(2);
        pl = in.btn.l; pr = in.btn.r; pc = in.btn.c_up;
    }
}

int gta2_n64_gfx_active(void);
void gta2_n64_print_mem(const char *tag);
static void print_mem(const char *tag) { gta2_n64_print_mem(tag); }
void gta2_n64_print_mem(const char *tag)
{
    struct mallinfo mi = mallinfo();
    {
        extern char end[]; /* fim do bss (linker) */
        void *brk = sbrk(0);
        static const unsigned sizes[] = {64, 128, 256, 512, 1024, 2048, 4096};
        printf("[mem %s] fim_bss=%08X brk=%08X  teste malloc(KB):", tag, (unsigned)(uintptr_t)end, (unsigned)(uintptr_t)brk);
        for (unsigned i = 0; i < sizeof(sizes) / sizeof(sizes[0]); i++) {
            void *p = malloc((size_t)sizes[i] * 1024);
            printf(" %u:%s", sizes[i], p ? "ok" : "NAO");
            free(p);
        }
        printf("\n");
    }
    printf("[mem %s] heap usado=%u KB, arena=%u KB, RAM=%u KB%s\n", tag, (unsigned)(mi.uordblks / 1024),
           (unsigned)(mi.arena / 1024), (unsigned)(get_memory_size() / 1024), is_memory_expanded() ? " (Expansion Pak)" : "");
}

int main(void)
{
    debug_init_isviewer();
    debug_init_usblog();
    display_init(RESOLUTION_320x240, DEPTH_16_BPP, 2, GAMMA_NONE, FILTERS_RESAMPLE);
    console_init();
    console_set_debug(true);
    console_set_render_mode(RENDER_MANUAL);
    /* resolucao: console_init() deixa a tela em 640x240 (mesma configuracao do build que ficou sem linhas) */
    timer_init();
    gta2_install_exception_fixups();
    joypad_init();
    int dfs = dfs_init(DFS_DEFAULT_LOCATION);

    printf("GTA2 N64 - boot do motor gta2_re (renderer nulo)\n");
    printf("dfs_init=%d\n", dfs);
    print_mem("antes");
    console_render();

    int rc = gta2_game_init();
    print_mem("apos init");
    { uint32_t ua, fp; gta2_exception_stats(&ua, &fp); printf("[fix] desalinhados=%lu fpu=%lu\n", (unsigned long)ua, (unsigned long)fp); }
    if (rc != 0)
    {
        printf("Motor saiu na inicializacao: codigo %d\n", rc);
        for (;;) { console_render(); }
    }
    printf("Init OK. Rodando o loop do jogo...\n");

    uint32_t frames = 0;
    for (;;)
    {
        poll_input();
        int r = gta2_game_frame();
        if (r != -1)
        {
            printf("Jogo terminou: codigo %d\n", r);
            print_mem("saida");
            for (;;) { console_render(); }
        }
        ++frames;
        if ((frames % 120) == 0)
        {
            { extern const uint32_t *gta2_port_input_stats(void);
              const uint32_t *st = gta2_port_input_stats();
              printf("[frame %lu] presents=%lu dinput[getData=%lu eventos=%lu getState=%lu acquire=%lu]\n", (unsigned long)frames, (unsigned long)gta2_game_presents(),
                     (unsigned long)st[0], (unsigned long)st[1], (unsigned long)st[2], (unsigned long)st[3]); }
            if ((frames % 600) == 0) { print_mem("loop"); uint32_t ua, fp; gta2_exception_stats(&ua, &fp); printf("[fix] desalinhados=%lu fpu=%lu\n", (unsigned long)ua, (unsigned long)fp); }
        }
        if (!gta2_n64_gfx_active()) console_render();
    }
}


/* ---- listagem de pasta do DragonFS com curinga (usada por _findfirst) ---- */
static int glob_match(const char *p, const char *s)
{
    for (; *p; ++p, ++s) {
        if (*p == '*') {
            while (p[1] == '*') ++p;
            if (!p[1]) return 1;
            for (; *s; ++s) if (glob_match(p + 1, s)) return 1;
            return glob_match(p + 1, s);
        }
        if (!*s) return 0;
        if (*p != '?' && *p != *s) return 0;
    }
    return !*s;
}

/* devolve 1 e copia o 1o nome que casa com 'glob' (minusculas) em 'out'; 0 se nada. */
int gta2_n64_dir_find(const char *dirpath, const char *glob, char *out, int outsz)
{
    dir_t d;
    int r = dir_findfirst(dirpath, &d);
    while (r == 0) {
        if (d.d_type != DT_DIR && glob_match(glob, d.d_name)) {
            strncpy(out, d.d_name, outsz - 1);
            out[outsz - 1] = 0;
            return 1;
        }
        r = dir_findnext(dirpath, &d);
    }
    return 0;
}

/* ---- rastreio de alocacoes grandes (ld --wrap): mostra quem consome a RAM ---- */
void *__real_malloc(size_t n);
void *__real_calloc(size_t a, size_t b);
void *__real_realloc(void *p, size_t n);
static int g_in_log;
static size_t g_big_total;
static void log_big(const char *what, size_t n, void *p, void *ra)
{
    if (g_in_log) return;
    g_in_log = 1;
    g_big_total += p ? n : 0;
    printf("[alloc] %s %uKB -> %s ra=%08X acum_grandes=%uKB\n", what, (unsigned)(n / 1024), p ? "ok" : "NULL",
           (unsigned)(uintptr_t)ra, (unsigned)(g_big_total / 1024));
    g_in_log = 0;
}
/* ---- verificador de integridade do heap do newlib (dlmalloc 2.6): acha o 1o bloco corrompido ---- */
extern unsigned *__malloc_av_[];
extern char *__malloc_sbrk_base;
static int g_hc_bad;          /* ja reportou: nao repete */
static unsigned g_hc_count;   /* chamadas de alocacao desde o boot */
static void *g_hc_last_ra;
int gta2_n64_heap_check(const char *tag, void *ra)
{
    if (g_hc_bad) return 1;
    unsigned lo = (unsigned)(uintptr_t)__malloc_sbrk_base;
    unsigned hi = (unsigned)(uintptr_t)sbrk(0);
    unsigned top = (unsigned)(uintptr_t)__malloc_av_[2];
    unsigned av0 = (unsigned)(uintptr_t)&__malloc_av_[0], av1 = av0 + 1032;
    unsigned p = (lo + 7) & ~7u;
    unsigned prev = 0, n = 0;
    while (p < top) {
        unsigned size = *(volatile unsigned *)(uintptr_t)(p + 4) & ~7u;
        unsigned nsz = (p + size < top) ? (*(volatile unsigned *)(uintptr_t)(p + size + 4)) : 1;
        int bad = (size < 16 || (size & 7) || p + size > hi);
        if (!bad && !(nsz & 1)) { /* bloco livre: fd/bk validos? */
            unsigned fd = *(volatile unsigned *)(uintptr_t)(p + 8), bk = *(volatile unsigned *)(uintptr_t)(p + 12);
            int okfd = (fd >= lo && fd < hi) || (fd >= av0 && fd < av1);
            int okbk = (bk >= lo && bk < hi) || (bk >= av0 && bk < av1);
            if (!okfd || !okbk) bad = 2;
        }
        if (bad) {
            g_hc_bad = 1;
            printf("[HEAP CORROMPIDO] tag=%s chamada#%u bloco=%08X (ant=%08X) tam=%u motivo=%d ra_check=%08X ultima_alloc_ra=%08X\n",
                   tag, g_hc_count, p, prev, size, bad, (unsigned)(uintptr_t)ra, (unsigned)(uintptr_t)g_hc_last_ra);
            printf("  palavras: ");
            for (int i = -4; i < 8; i++) printf("%08X ", *(volatile unsigned *)(uintptr_t)(p + i * 4));
            printf("\n  ant(+0..): ");
            for (int i = 0; i < 6; i++) printf("%08X ", *(volatile unsigned *)(uintptr_t)(prev + i * 4));
            printf("\n");
            return 1;
        }
        prev = p; p += size; n++;
    }
    return 0;
}
static void hc_tick(void *ra)
{
    g_hc_last_ra = ra;
    if (g_in_log) return;
    ++g_hc_count;
    if ((g_hc_count & 31) == 0 || g_big_total >= 9800u * 1024u) gta2_n64_heap_check("periodico", ra);
}
void *__wrap_malloc(size_t n)
{
    hc_tick(__builtin_return_address(0));
    void *p = __real_malloc(n);
    if (n >= 65536 || !p) log_big("malloc", n, p, __builtin_return_address(0));
    return p;
}
void *__wrap_calloc(size_t a, size_t b)
{
    hc_tick(__builtin_return_address(0));
    void *p = __real_calloc(a, b);
    if (a * b >= 65536 || !p) log_big("calloc", a * b, p, __builtin_return_address(0));
    return p;
}
void *__wrap_realloc(void *q, size_t n)
{
    hc_tick(__builtin_return_address(0));
    void *p = __real_realloc(q, n);
    if (n >= 65536 || !p) log_big("realloc", n, p, __builtin_return_address(0));
    return p;
}

/* endereco CPU (KSEG1, sem cache) de um arquivo do DragonFS + offset; NULL se nao achar */
const void *gta2_n64_rom_ptr(const char *romPath, unsigned offset)
{
    static int shown;
    uint32_t a = dfs_rom_addr(romPath);
    if (!a && strncmp(romPath, "rom:/", 5) == 0) a = dfs_rom_addr(romPath + 5);
    if (!a) return NULL;
    uint32_t phys = a & 0x1FFFFFFF;
    if (phys < 0x10000000u) phys += 0x10000000u;      /* veio como offset na ROM */
    if (!shown++) printf("[rom-map] %s dfs_rom_addr=%08X -> ptr=%08X\n", romPath, (unsigned)a, (unsigned)(0xA0000000u | (phys + offset)));
    return (const void *)(uintptr_t)(0xA0000000u | (phys + offset));
}
