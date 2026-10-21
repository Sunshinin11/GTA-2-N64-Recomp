/* Som do GTA 2 no N64: os efeitos que o motor dispara pela camada do Miles (miles_n64.cpp) tocam no mixer da
 * libdragon (mistura no RSP). Os samples ficam na ROM (banco .raw mapeado, ja em big-endian) e cada canal le
 * so o trecho que esta tocando, por DMA. Nada do banco ocupa RAM.
 *
 * Suporta PCM 16 bits mono (formato dos bancos .raw do GTA 2). Formatos de 8 bits (sem sinal no Miles) e
 * estereo sao ignorados. Sons pedidos antes do audio iniciar (gta2_n64_audio_init) sao ignorados.
 */
#include <libdragon.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>

#define NVOICE      20    /* canais do mixer para efeitos (0..19) */
#define NSTREAM     2     /* streams do Miles: 0 = radio, 1 = falas; canais NVOICE.. */
#define NWAVE       384   /* formas de onda distintas (banco tem 320 entradas) */
#define OUT_FREQ    32000
#define MAX_FREQ    66150.0f /* teto por canal: 3x 22050 Hz (o jogo sobe o tom dos efeitos, ex.: 50292 Hz) */

typedef struct {
    const uint8_t *data;  /* inicio do sample (ROM KSEG1 ou RAM) */
    uint32_t len;         /* bytes */
    int32_t loop_start, loop_end; /* bytes, relativos ao inicio; loop_end -1 = fim */
    int loops;            /* 0 = infinito */
    waveform_t wf;
} Wave;

typedef struct {
    int id;               /* handle do Miles (0 = livre) */
    uint32_t order;       /* para roubar o canal mais antigo */
} Voice;

static Wave g_wave[NWAVE];
static int g_nwave;
static Voice g_voice[NVOICE];
static uint32_t g_order;
static int g_inited;

extern void gta2_n64_rom_copy(void *dst, const void *src, unsigned len); /* render_n64.c */

static inline int in_rom(const void *p) { const uintptr_t a = (uintptr_t)p; return a >= 0xB0000000u && a < 0xC0000000u; }

static void wave_read(void *ctx, samplebuffer_t *sbuf, int wpos, int wlen, bool seeking)
{
    (void)seeking;
    Wave *w = (Wave *)ctx;
    uint8_t *dst = (uint8_t *)samplebuffer_append(sbuf, wlen);
    const uint32_t off = (uint32_t)wpos * 2u;
    uint32_t n = (uint32_t)wlen * 2u, have = 0;
    if (off < w->len) have = w->len - off;
    if (have > n) have = n;
    if (have) {
        const uint8_t *src = w->data + off;
        if (in_rom(src) && (((uintptr_t)src ^ (uintptr_t)dst) & 1u) == 0u)
            dma_read(dst, (uint32_t)((uintptr_t)src & 0x1FFFFFFFu), have); /* espera o PI e le em DMA */
        else
            gta2_n64_rom_copy(dst, src, have);                              /* RAM, ou paridade diferente */
    }
    if (have < n) memset(dst + have, 0, n - have);
}

static Wave *wave_get(const void *data, uint32_t len, int loops, int32_t ls, int32_t le)
{
    for (int i = 0; i < g_nwave; ++i) {
        Wave *w = &g_wave[i];
        if (w->data == data && w->len == len && w->loops == loops && w->loop_start == ls && w->loop_end == le) return w;
    }
    if (g_nwave >= NWAVE) return NULL;
    Wave *w = &g_wave[g_nwave++];
    memset(w, 0, sizeof(*w));
    w->data = (const uint8_t *)data; w->len = len; w->loops = loops; w->loop_start = ls; w->loop_end = le;
    w->wf.name = "gta2";
    w->wf.bits = 16;
    w->wf.channels = 1;
    w->wf.frequency = 22050.0f;
    w->wf.len = (int)(len / 2u);
    if (loops == 0) { /* Miles: 0 = repete ate o jogo parar o som */
        int32_t s = ls > 0 ? ls : 0, e = (le < 0 || (uint32_t)le > len) ? (int32_t)len : le;
        if (e - s >= 4) {
            w->wf.loop_len = (e - s) / 2;
            w->wf.loop_end = (e == (int32_t)len) ? 0 : e / 2;
        }
    }
    w->wf.read = wave_read;
    w->wf.ctx = w;
    return w;
}

static int voice_find(int id)
{
    for (int c = 0; c < NVOICE; ++c) if (g_voice[c].id == id) return c;
    return -1;
}

static void apply_mix(int ch, int volume, int pan, int rate)
{
    float v = (float)volume / 127.0f, p = (float)pan / 127.0f;
    if (v < 0.0f) v = 0.0f;
    if (v > 1.0f) v = 1.0f;
    if (p < 0.0f) p = 0.0f;
    if (p > 1.0f) p = 1.0f;
    mixer_ch_set_vol_pan(ch, v, p);
    if (rate > 0) {
        float f = (float)rate;
        if (f > MAX_FREQ) f = MAX_FREQ; /* acima do limite o mixer para o jogo (assert) */
        mixer_ch_set_freq(ch, f);
    }
}

void gta2_n64_audio_init(void)
{
    if (g_inited) return;
    audio_init(OUT_FREQ, AUDIO_DEFAULT_LATENCY);
    mixer_init(NVOICE + NSTREAM);
    for (int c = 0; c < NVOICE + NSTREAM; ++c) mixer_ch_set_limits(c, 16, MAX_FREQ, 0);
    wav64_init_compression(2); /* radios em ULC (audioconv64 --wav-compress ulc) */
    g_inited = 1;
}

/* alimenta o AI: chamado algumas vezes por quadro (render_n64.c) */
void gta2_n64_audio_poll(void)
{
    if (!g_inited) return;
    while (audio_can_write()) {
        short *buf = audio_write_begin();
        mixer_poll(buf, audio_get_buffer_length());
        audio_write_end();
    }
}

void gta2_n64_sfx_start(int id, const void *data, int len, int rate, int format, int loops, int loop_start,
                        int loop_end, int volume, int pan)
{
    if (!g_inited || !id || len < 4) return;
    if (!(format & 1) || (format & 2)) return;          /* so PCM 16 bits mono */
    if (!in_rom(data)) return;                           /* banco ainda nao mapeado da ROM */
    Wave *w = wave_get(data, (uint32_t)len & ~1u, loops, loop_start, loop_end);
    if (!w) return;
    int ch = voice_find(id);
    if (ch < 0) for (int c = 0; c < NVOICE; ++c) if (!mixer_ch_playing(c)) { ch = c; break; }
    if (ch < 0) { /* todos ocupados: rouba o mais antigo */
        ch = 0;
        for (int c = 1; c < NVOICE; ++c) if (g_voice[c].order < g_voice[ch].order) ch = c;
    }
    for (int c = 0; c < NVOICE; ++c) if (c != ch && g_voice[c].id == id) g_voice[c].id = 0;
    g_voice[ch].id = id;
    g_voice[ch].order = ++g_order;
    mixer_ch_play(ch, &w->wf);
    apply_mix(ch, volume, pan, rate);
}

void gta2_n64_sfx_update(int id, int volume, int pan, int rate)
{
    if (!g_inited) return;
    const int ch = voice_find(id);
    if (ch >= 0 && mixer_ch_playing(ch)) apply_mix(ch, volume, pan, rate);
}

void gta2_n64_sfx_stop(int id)
{
    if (!g_inited) return;
    const int ch = voice_find(id);
    if (ch >= 0) { mixer_ch_stop(ch); g_voice[ch].id = 0; }
}

void gta2_n64_master_volume(int volume)
{
    if (!g_inited) return;
    float v = (float)volume / 127.0f;
    if (v < 0.0f) v = 0.0f;
    if (v > 1.0f) v = 1.0f;
    mixer_set_vol(v);
}

/* ---- Streams (radios em gtaudio, arquivos .wav64 convertidos no build) ---- */
typedef struct {
    int id;          /* handle do Miles (0 = livre) */
    int open;
    wav64_t wav;
    int volume, rate;
} Strm;
static Strm g_st[NSTREAM];

static Strm *stream_find(int id)
{
    for (int i = 0; i < NSTREAM; ++i) if (g_st[i].open && g_st[i].id == id) return &g_st[i];
    return NULL;
}
static inline int stream_ch(const Strm *s) { return NVOICE + (int)(s - g_st); }

static void stream_apply(Strm *s)
{
    const int ch = stream_ch(s);
    float v = (float)s->volume / 127.0f;
    if (v < 0.0f) v = 0.0f;
    if (v > 1.0f) v = 1.0f;
    mixer_ch_set_vol(ch, v, v);
    if (s->rate > 0 && s->rate != 22050) {
        float f = s->wav.wave.frequency * (float)s->rate / 22050.0f;
        if (f > MAX_FREQ) f = MAX_FREQ;
        mixer_ch_set_freq(ch, f);
    }
}

/* path ja normalizado pelo port ("rom:/data/gtaudio/1.wav"); o arquivo no DFS e' o .wav64 */
int gta2_n64_stream_open(int id, const char *path)
{
    if (!g_inited || !id || !path) return 0;
    char p[200];
    const size_t n = strlen(path);
    if (n + 3 >= sizeof(p)) return 0;
    memcpy(p, path, n + 1);
    if (n >= 4 && strcmp(p + n - 4, ".wav") == 0) strcat(p, "64");
    FILE *f = fopen(p, "rb");           /* wav64_open para o jogo se o arquivo nao existir */
    fprintf(stderr, "[radio] stream_open id=%08X %s existe=%d\n", (unsigned)id, p, f != NULL); /* DIAGNOSTICO TEMPORARIO */
    if (!f) return 0;
    fclose(f);
    Strm *s = stream_find(id);
    if (!s) for (int i = 0; i < NSTREAM; ++i) if (!g_st[i].open) { s = &g_st[i]; break; }
    if (!s) return 0;
    if (s->open) { mixer_ch_stop(stream_ch(s)); wav64_close(&s->wav); s->open = 0; }
    wav64_open(&s->wav, p);
    s->id = id; s->open = 1; s->volume = 127; s->rate = 22050;
    fprintf(stderr, "[radio] aberto canal=%d freq=%.0f len=%d bits=%d ch=%d\n", stream_ch(s), (double)s->wav.wave.frequency, s->wav.wave.len, s->wav.wave.bits, s->wav.wave.channels); /* DIAGNOSTICO TEMPORARIO */
    return 1;
}

void gta2_n64_stream_close(int id)
{
    Strm *s = stream_find(id);
    if (!s) return;
    mixer_ch_stop(stream_ch(s));
    wav64_close(&s->wav);
    s->open = 0; s->id = 0;
}

void gta2_n64_stream_start(int id, int loops)
{
    Strm *s = stream_find(id);
    if (!s) { fprintf(stderr, "[radio] start id=%08X sem stream aberto\n", (unsigned)id); return; } /* DIAGNOSTICO TEMPORARIO */
    wav64_set_loop(&s->wav, loops == 0); /* Miles: 0 = repete sempre (radio) */
    wav64_play(&s->wav, stream_ch(s));
    stream_apply(s);
    fprintf(stderr, "[radio] start id=%08X canal=%d loop=%d tocando=%d vol=%d\n", (unsigned)id, stream_ch(s), loops == 0, (int)mixer_ch_playing(stream_ch(s)), s->volume); /* DIAGNOSTICO TEMPORARIO */
}

int gta2_n64_stream_playing(int id)
{
    Strm *s = stream_find(id);
    return s && mixer_ch_playing(stream_ch(s));
}

void gta2_n64_stream_set(int id, int volume, int rate)
{
    Strm *s = stream_find(id);
    if (!s) return;
    s->volume = volume; s->rate = rate;
    if (mixer_ch_playing(stream_ch(s))) stream_apply(s);
}

void gta2_n64_stream_seek(int id, int ms)
{
    Strm *s = stream_find(id);
    if (!s || ms < 0) return;
    if (mixer_ch_playing(stream_ch(s))) wav64_seek(&s->wav, stream_ch(s), (double)ms / 1000.0);
}

/* which: 0 = duracao total, 1 = posicao atual (ms) */
int gta2_n64_stream_ms(int id, int which)
{
    Strm *s = stream_find(id);
    if (!s || s->wav.wave.frequency <= 0.0f) return 0;
    const double f = s->wav.wave.frequency;
    if (which == 0) return s->wav.wave.len > 0 ? (int)(s->wav.wave.len * 1000.0 / f) : 0;
    return mixer_ch_playing(stream_ch(s)) ? (int)(mixer_ch_get_pos(stream_ch(s)) * 1000.0 / f) : 0;
}
