// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

bool JumpHook32(BYTE* targetAddress, BYTE* hookFunction, size_t	length);
BYTE* Trampoline32(BYTE* targetAddress, BYTE* trampolineFunction, size_t length);

bool JumpHook64(BYTE* targetAddress, BYTE* hookFunction, size_t	length);
BYTE* Trampoline64(BYTE* targetAddress, BYTE* trampolineFunction, size_t length);

bool HookAPI32(HMODULE hModule, const char* functionName, void** original_Function, void* hooked_Function, int	trampolineSize);
bool PatchIATEntry(HMODULE hModule, const char* targetDLL, const char* originalFunc, PROC hookedFunc, PROC& originalFuncOut);