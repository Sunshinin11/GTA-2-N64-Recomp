#pragma once
// libstdc++ antes dos #define de wcs*/vswprintf do port_prefix.h (senao quebram dentro de std::)
#include <string>
#include <fstream>
#include <sstream>
#include <vector>
#include <cwchar>
#define __int64 long long
#define __declspec(x)
#define __cdecl
#define __stdcall
#define __fastcall
#define __thiscall
using std::ofstream; using std::ifstream; using std::fstream; using std::ios;
#define __int8 char
#define __int16 short
#define __int32 int
#define __forceinline inline __attribute__((always_inline))
#define static_assert(...)
