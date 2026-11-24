// SPDX-License-Identifier: GPL-3.0-or-later

#include "hookslib.h"
#include <string.h>
#include <Windows.h>
#include <cstdarg>
#include <string>
#include <spdlog/spdlog.h>
#include "logging.h"
#include <cstdint>

bool JumpHook32(BYTE* hookAddress, BYTE* hookFunction, const size_t length) {
	if (!hookAddress || !hookFunction || length < 5) { return false; }

	DWORD oldProtect;
	if (!VirtualProtect(hookAddress, length, PAGE_EXECUTE_READWRITE, &oldProtect)) { return false; }

	memset(hookAddress, 0x90, length);

	intptr_t relativeAddress = (intptr_t)(hookFunction - (hookAddress + 5));

	*hookAddress = 0xE9;
	*(int32_t*)(hookAddress + 1) = (int32_t)relativeAddress;

	VirtualProtect(hookAddress, length, oldProtect, &oldProtect);

	FlushInstructionCache(GetCurrentProcess(), hookAddress, length);
	return true;
}

BYTE* Trampoline32(BYTE* trampolineAddress, BYTE* destinationAddress, const size_t length) {
	if (!trampolineAddress || !destinationAddress || length < 5) { return nullptr; }

	BYTE* gateway = (BYTE*)VirtualAlloc(nullptr, length + 5, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
	if (!gateway) { return nullptr; }

	memset(gateway, 0, length + 5);
	memcpy_s(gateway, length, trampolineAddress, length);

	intptr_t gatewayRelativeAddress =
		(intptr_t)((trampolineAddress + length) - (gateway + length + 5));

	*(gateway + length) = 0xE9;
	*(int32_t*)(gateway + length + 1) = (int32_t)gatewayRelativeAddress;

	if (!JumpHook32(trampolineAddress, destinationAddress, length)) {
		VirtualFree(gateway, 0, MEM_RELEASE);
		return nullptr;
	}

	FlushInstructionCache(GetCurrentProcess(), gateway, length + 5);
	return gateway;
}

bool JumpHook64(BYTE* hookAddress, BYTE* hookFunction, const size_t length) {
	if (!hookAddress || !hookFunction || length < 12) { return false; }

	DWORD oldProtect;
	if (!VirtualProtect(hookAddress, length, PAGE_EXECUTE_READWRITE, &oldProtect)) { return false; }

	memset(hookAddress, 0x90, length);

	*(hookAddress + 0) = 0x48;	
	*(hookAddress + 1) = 0xB8;
	*(uint64_t*)(hookAddress + 2) = (uint64_t)hookFunction;
	*(hookAddress + 10) = 0xFF;
	*(hookAddress + 11) = 0xE0;

	VirtualProtect(hookAddress, length, oldProtect, &oldProtect);

	FlushInstructionCache(GetCurrentProcess(), hookAddress, length);
	return true;
}

BYTE* Trampoline64(BYTE* trampolineAddress, BYTE* destinationAddress, const size_t length) {
	if (!trampolineAddress || !destinationAddress || length < 12) { return nullptr; }

	BYTE* gateway = (BYTE*)VirtualAlloc(nullptr, length + 12, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
	if (!gateway) { return nullptr; }

	memset(gateway, 0, length + 12);
	memcpy_s(gateway, length, trampolineAddress, length);

	*(gateway + length + 0) = 0x48;
	*(gateway + length + 1) = 0xB8;
	*(uint64_t*)(gateway + length + 2) = (uint64_t)(trampolineAddress + length);

	*(gateway + length + 10) = 0xFF;
	*(gateway + length + 11) = 0xE0;

	if (!JumpHook64(trampolineAddress, destinationAddress, length)) {
		VirtualFree(gateway, 0, MEM_RELEASE);
		return nullptr;
	}

	FlushInstructionCache(GetCurrentProcess(), gateway, length + 12);
	return gateway;
}

bool HookAPI32(HMODULE hModule, const char* functionName, void** original_Function, void* hooked_Function, int trampolineSize) {
	void* targetFunction = GetProcAddress(hModule, functionName);
	if (!targetFunction) {
		spdlog::error("HookAPI32: Failed to find function {} in module {}.", functionName, hModule ? std::to_string((uintptr_t)hModule) : "nullptr");
		return false;
	}
	*original_Function = (void*)Trampoline32((BYTE*)targetFunction, (BYTE*)hooked_Function, trampolineSize);
	if (*original_Function) { return true; }
	else { return false; }
}

bool PatchIATEntry(HMODULE hModule, const char* targetDLL, const char* originalFunc, PROC hookedFunc, PROC &originalFuncOut) {
	if (!hModule || !targetDLL || !originalFunc || !hookedFunc) { return false; }

	const PBYTE baseAddress = reinterpret_cast<PBYTE>(hModule);
	PIMAGE_DOS_HEADER dosHeader = reinterpret_cast<PIMAGE_DOS_HEADER>(baseAddress);
	if (dosHeader->e_magic != IMAGE_DOS_SIGNATURE) { return false; }

	PIMAGE_NT_HEADERS ntHeaders = reinterpret_cast<PIMAGE_NT_HEADERS>(baseAddress + dosHeader->e_lfanew);
	if (ntHeaders->Signature != IMAGE_NT_SIGNATURE) { return false; }

	PIMAGE_DATA_DIRECTORY importDir = &ntHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
	if (!importDir->Size) { return false; }

	PIMAGE_IMPORT_DESCRIPTOR importDesc = reinterpret_cast<PIMAGE_IMPORT_DESCRIPTOR>(baseAddress + importDir->VirtualAddress);

	for (PIMAGE_IMPORT_DESCRIPTOR desc = importDesc; desc->Name != 0; ++desc) {
		const char* moduleName = reinterpret_cast<const char*>(baseAddress + desc->Name);
		if (_stricmp(moduleName, targetDLL) != 0) { continue; }

		PIMAGE_THUNK_DATA thunk = reinterpret_cast<PIMAGE_THUNK_DATA>(baseAddress + desc->FirstThunk);
		PIMAGE_THUNK_DATA origThunk = reinterpret_cast<PIMAGE_THUNK_DATA>(baseAddress + desc->OriginalFirstThunk);

		for (; origThunk->u1.AddressOfData != 0; ++origThunk, ++thunk) {
			if (origThunk->u1.Ordinal & IMAGE_ORDINAL_FLAG) {
				continue;
			}

			PIMAGE_IMPORT_BY_NAME importName = reinterpret_cast<PIMAGE_IMPORT_BY_NAME>(baseAddress + origThunk->u1.AddressOfData);
			const char* funcName = reinterpret_cast<const char*>(importName->Name);

			if (_stricmp(funcName, originalFunc) == 0) {
				PROC* pFunc = reinterpret_cast<PROC*>(&thunk->u1.Function);
				DWORD oldProtect;
				if (!VirtualProtect(pFunc, sizeof(PROC), PAGE_READWRITE, &oldProtect)) { return false; }
				originalFuncOut = *pFunc;
				*pFunc = hookedFunc;
				VirtualProtect(pFunc, sizeof(PROC), oldProtect, &oldProtect);
				spdlog::trace("Patched IAT: {} ({}): 0x{:X} -> 0x{:X}", originalFunc, targetDLL, (uintptr_t)originalFuncOut, (uintptr_t)hookedFunc);
				return true;
			}
		}
	}
	spdlog::error("Failed to patch IAT: {} ({}).", originalFunc, targetDLL);
	return false;
}

static bool PatchIATEntry(HMODULE hModule, const char* targetDLL, WORD originalFuncOrdinal, PROC hookedFunc, PROC& originalFuncOut) {
	if (!hModule || !targetDLL || !hookedFunc) { return false; }

	const PBYTE baseAddress = reinterpret_cast<PBYTE>(hModule);
	PIMAGE_DOS_HEADER dosHeader = reinterpret_cast<PIMAGE_DOS_HEADER>(baseAddress);
	if (dosHeader->e_magic != IMAGE_DOS_SIGNATURE) { return false; }

	PIMAGE_NT_HEADERS ntHeaders = reinterpret_cast<PIMAGE_NT_HEADERS>(baseAddress + dosHeader->e_lfanew);
	if (ntHeaders->Signature != IMAGE_NT_SIGNATURE) { return false; }

	PIMAGE_DATA_DIRECTORY importDir = &ntHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
	if (!importDir->Size) { return false; }

	PIMAGE_IMPORT_DESCRIPTOR importDesc = reinterpret_cast<PIMAGE_IMPORT_DESCRIPTOR>(baseAddress + importDir->VirtualAddress);

	for (PIMAGE_IMPORT_DESCRIPTOR desc = importDesc; desc->Name != 0; ++desc) {
		const char* moduleName = reinterpret_cast<const char*>(baseAddress + desc->Name);
		if (_stricmp(moduleName, targetDLL) != 0) { continue; }

		PIMAGE_THUNK_DATA thunk = reinterpret_cast<PIMAGE_THUNK_DATA>(baseAddress + desc->FirstThunk);
		PIMAGE_THUNK_DATA origThunk = reinterpret_cast<PIMAGE_THUNK_DATA>(baseAddress + desc->OriginalFirstThunk);

		for (; origThunk->u1.AddressOfData != 0; ++origThunk, ++thunk) {
			if (!(origThunk->u1.Ordinal & IMAGE_ORDINAL_FLAG)) {
				continue;
			}

			WORD ordinal = IMAGE_ORDINAL(origThunk->u1.Ordinal);

			if (ordinal == originalFuncOrdinal) {
				PROC* pFunc = reinterpret_cast<PROC*>(&thunk->u1.Function);
				DWORD oldProtect;
				if (!VirtualProtect(pFunc, sizeof(PROC), PAGE_READWRITE, &oldProtect)) { return false; }
				originalFuncOut = *pFunc;
				*pFunc = hookedFunc;
				VirtualProtect(pFunc, sizeof(PROC), oldProtect, &oldProtect);

				spdlog::debug("Patched IAT: Ordinal {} ({}): 0x{:X} -> 0x{:X}", originalFuncOrdinal, targetDLL, (uintptr_t)originalFuncOut, (uintptr_t)hookedFunc);
				return true;
			}
		}
	}
	spdlog::error("Failed to patch IAT: Ordinal {} ({}).", originalFuncOrdinal, targetDLL);
	return false;
}