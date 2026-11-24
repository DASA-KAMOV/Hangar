// SPDX-License-Identifier: GPL-3.0-or-later

#include "memory.h"
#include <string.h>
#include <Windows.h>
#include <cstdint>
#include <initializer_list>

bool NOPMemory(BYTE* destinationAddress, size_t length) {
	DWORD oldProtect;
	if (VirtualProtect(destinationAddress, length, PAGE_EXECUTE_READWRITE, &oldProtect) == 0) { return false; }
	memset(destinationAddress, 0x90, length);
	DWORD tempProtect;
	if (VirtualProtect(destinationAddress, length, oldProtect, &tempProtect) == 0) { return false; }
	return true;
}

bool PatchMemory(BYTE* destinationAddress, BYTE* patchBytes, size_t length) {
	DWORD oldProtect;
	if (VirtualProtect(destinationAddress, length, PAGE_EXECUTE_READWRITE, &oldProtect) == 0) { return false; }
	if (memcpy_s(destinationAddress, length, patchBytes, length) != 0) { return false; };
	DWORD tempProtect;
	if (VirtualProtect(destinationAddress, length, oldProtect, &tempProtect) == 0) { return false; }
	return true;
}

bool PatchMemory(BYTE* destinationAddress, std::initializer_list<BYTE> bytes){ return PatchMemory(destinationAddress, (BYTE*)bytes.begin(), bytes.size()); }

bool IsValidExecutableMemory(BYTE* address) {
	MEMORY_BASIC_INFORMATION mbi = { 0 };
	if (VirtualQuery(address, &mbi, sizeof(mbi))) { return (mbi.State == MEM_COMMIT) && (mbi.Protect & PAGE_EXECUTE_READ); }
	return false;
}

// Useage:
// JumpPatch myPatch = {};
// ToggleAnyCondJmpToJmp((BYTE*)0x00C0FFEE, myPatch, true);
bool ToggleCondJMPToJMP(BYTE* address, JumpPatch& jumpPatch, bool enable) {
	if (enable) {
		if (!jumpPatch.saved) {
			memcpy_s(jumpPatch.original, 6, address, 2);

			if (jumpPatch.original[0] == 0x0F && (jumpPatch.original[1] & 0xF0) == 0x80) {
				memcpy_s(jumpPatch.original, 6, address, 6);
				jumpPatch.length = 6;
			}  // Near JMP: 0F 8x rel32
			else if ((jumpPatch.original[0] & 0xF0) == 0x70) { jumpPatch.length = 2; } // Short JMP: 7x rel8
			else { return false; }

			jumpPatch.saved = true;
		}

		if (jumpPatch.length == 2) {
			BYTE patch[2] = { 0xEB, jumpPatch.original[1] };
			return PatchMemory(address, patch, sizeof(patch));
		}

		BYTE patch[6]{};
		BYTE* end = address + jumpPatch.length;
		BYTE* target;

		if (jumpPatch.length == 6) {
			int32_t rel32 = *(int32_t*)(jumpPatch.original + 2);
			target = end + rel32;
		}
		else {
			int8_t rel8 = *(int8_t*)(jumpPatch.original + 1);
			target = end + rel8;
		}

		// Build: 
		// JMP <rel32>
		// NOP
		patch[0] = 0xE9;
		BYTE* jmpEnd = address + 5;
		int32_t newRel = (int32_t)(target - jmpEnd);
		memcpy_s(patch + 1, 4, &newRel, 4);
		patch[5] = 0x90;

		return PatchMemory(address, patch, 6);
	}
	else {
		if (!jumpPatch.saved) { return false; }
		return PatchMemory(address, jumpPatch.original, jumpPatch.length);
	}
}