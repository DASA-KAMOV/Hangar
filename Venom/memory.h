// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <type_traits>
#include <initializer_list>
#include <string.h>
#include <cstring>

struct JumpPatch {
	BYTE original[6];
	size_t length;
	bool saved;
};

bool NOPMemory(BYTE* destinationAddress, size_t size);
bool PatchMemory(BYTE* destinationAddress, BYTE* patch, size_t size);
bool PatchMemory(BYTE* destinationAddress, std::initializer_list<BYTE> bytes);
bool IsValidExecutableMemory(BYTE* address);
bool ToggleCondJMPToJMP(BYTE* address, JumpPatch &jumpPatch, bool enable);

template <typename T>
bool PatchMemory(BYTE* destinationAddress, const T &value) {
	static_assert(std::is_trivially_copyable<T>::value, "PatchMemory<T>: T must be trivially copyable");
    BYTE* bytes = (BYTE*)&value;
    return PatchMemory(destinationAddress, bytes, sizeof(T));
}

template <size_t N>
bool PatchMemory(BYTE* destinationAddress, const BYTE(&arr)[N]) { return PatchMemory(destinationAddress, (BYTE*)arr, N); }

template<size_t N>
void* FindSignatureInMemory(BYTE* moduleBase, size_t moduleSize, const BYTE(&signature)[N]) {
    if (!moduleBase || N == 0 || moduleSize < N) { return nullptr; }

    const BYTE first = signature[0];

    for (size_t i = 0; i <= moduleSize - N; i++) {
        if (moduleBase[i] != first) { continue; }
        if (std::memcmp(moduleBase + i, signature, N) == 0) { return moduleBase + i; }
    }

    return nullptr;
}

template<size_t N>
void* FindSignatureInMemory(BYTE* moduleBase, size_t moduleSize, const BYTE(&signature)[N], const BYTE(&mask)[N]) {
    static_assert(N == sizeof(mask), "FindSignatureInMemory: Signature and Mask must be same size!");

    if (!moduleBase || moduleSize < N) { return nullptr; }

    const BYTE firstMask = mask[0];
    const BYTE firstSig = signature[0];

    for (size_t i = 0; i <= moduleSize - N; i++) {
        if (firstMask && moduleBase[i] != firstSig) { continue; }

        bool match = true;

        for (size_t j = 1; j < N; j++) {
            if (mask[j] && moduleBase[i + j] != signature[j]) {
                match = false;
                break;
            }
        }

        if (match) { return moduleBase + i; }
    }
    return nullptr;
}

template<size_t N>
BYTE* FindNullTerminatedStringInMemory(BYTE* base, size_t moduleSize, const BYTE(&signature)[N]) {
    BYTE* loc = (BYTE*)FindSignatureInMemory(base, moduleSize, signature);
    if (!loc) { return nullptr; }

    BYTE* start = loc;
    while (start > base && start[-1] != 0x00) { --start; }

    return start;
}
