// Descritor de textura para o renderer N64: o motor (gbh_n64.cpp) preenche, o renderer (render_n64.c) le.
// O ponteiro para este bloco fica em STexture::field_1C_ptr (serve de "id" da textura nos comandos de desenho).
#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct N64Tex
{
    const uint8_t* pix;   // pixels 8 bits (paleta), linhas de 256 bytes (paginas do .sty)
    const uint32_t* pal;  // paleta fisica: entrada i em pal[i*64], 0x00RRGGBB
    uint16_t w, h;
    uint16_t gen;         // sobe a cada (re)preenchimento: invalida a copia convertida
    int16_t slot;         // posicao no cache de texturas convertidas (-1 = nenhuma)
} N64Tex;
#ifdef __cplusplus
}
#endif
