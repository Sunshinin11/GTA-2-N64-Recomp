// Shim minimo de <windows.h> para compilar o gta2_re fora do Windows (N64 / teste com g++).
#include "compat.h"
#pragma once
#include <stdint.h>
#include <stddef.h>
typedef int32_t BOOL; typedef uint8_t BYTE; typedef uint16_t WORD; typedef uint32_t DWORD;
typedef int32_t LONG; typedef uint32_t UINT; typedef int32_t INT; typedef void* HANDLE; typedef void* HWND;
typedef void* HINSTANCE; typedef void* HMODULE; typedef void* LPVOID; typedef const void* LPCVOID;
typedef char* LPSTR; typedef const char* LPCSTR; typedef long LRESULT; typedef unsigned long WPARAM; typedef long LPARAM;
typedef struct { LONG x, y; } POINT; typedef struct { LONG left, top, right, bottom; } RECT;
#define TRUE 1
#define FALSE 0
#define WINAPI
#define CALLBACK
#define MAX_PATH 260
