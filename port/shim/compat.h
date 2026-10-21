// Definicoes MSVC6 que o gta2_re assume. Force-included (-include) em todo arquivo.
#pragma once
#include <stdint.h>
#include <stddef.h>
#ifndef __int64
#define __int64 long long
#endif
#define __declspec(x)
#define __cdecl
#define __stdcall
#define __fastcall
#define __thiscall
#define __forceinline inline __attribute__((always_inline))
