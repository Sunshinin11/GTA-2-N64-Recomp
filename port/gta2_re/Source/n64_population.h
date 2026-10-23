#pragma once
// Populacao do tech demo N64: percentual da quantidade ORIGINAL de pedestres e carros (100 = igual ao PC).
// Ajuste so este numero: 60 = 40% menos gente e carros. Pode passar -DN64_POP_PCT=50 no Makefile.
#ifdef N64
#ifndef N64_POP_PCT
#define N64_POP_PCT 40
#endif
#else
#define N64_POP_PCT 100
#endif

// 1 = liga os logs de diagnostico de jogador/personagem/entrada (poluem o log e custam CPU)
#ifndef N64_DEBUG_LOGS
#define N64_DEBUG_LOGS 0
#endif
