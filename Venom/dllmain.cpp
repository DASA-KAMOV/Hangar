// SPDX-License-Identifier: GPL-3.0-or-later

// Currently using inline assembly, which is not supported on x64 or ARM. May rework in the future to support x64 and ARM, but for now, this is an x86-only DLL.
static_assert(sizeof(void*) == 4, "Venom.dll is x86-only - inline assembly not supported by x64 or ARM.");

#pragma once
#define WIN32_LEAN_AND_MEAN
#include "config.h"
#include "hookslib.h"
#include "logging.h"
#include "memory.h"
#include "hookedWindowsFunctions.h"
#include <iostream>
#include <unordered_map>
#include <cstdarg>
#include <cstdint>
#include <string>
#include <windows.h>
#include <Psapi.h>
#include <spdlog/common-inl.h>
#include <spdlog/common.h>
#include <spdlog/fmt/std.h>
#include <spdlog/spdlog.h>
#include <spdlog/spdlog-inl.h>
#include "musicPlayer.h"
#include <string.h>
#include <chrono>
#include <vector>
#include <cctype>

std::unordered_map<int, bool> keyStates = {};
std::unordered_map<std::string, std::string> configValues = {};

bool showConsole = true;
bool enableAllCameraModes = true;
bool enableAssetOverride = false;
bool enableGraphicsProxy = false;
bool enablePOVtoKeyboard = true;
bool virtualCD = false;
bool subtitlesEnabled = false;
bool textTweaks = true;

bool fixBleeps = true;
bool fixFocus = true;
bool fixIcon = true;
bool fixMutex = false;
bool drawExtraCockpit = true;
int32_t terrainLODThreshold = 25000; // 25000 (0xA861) is the G-Police default.
int32_t civCarSpawnDelay = 4500; // 4500 (0x1194) is the G-Police default.

bool bombingMode = false;
bool psuedoWireframe = false;
bool noHUD = false;

float FPSLimit = 0.0;

spdlog::level::level_enum consoleLogLevel = spdlog::level::info;
spdlog::level::level_enum fileLogLevel = spdlog::level::trace;
std::string logFilepath = "";

bool enableAudioProxy = false;
bool useDA = false;
bool enableMediaKeys = false;
MusicPlayer musicPlayer;

int failCount = 0;
int successCount = 0;

HMODULE hKernel32 = nullptr;
HANDLE threadHandle = nullptr;

HMODULE moduleBaseAddress = nullptr;
MODULEINFO moduleInfo = {};

HWND storedHWND = nullptr;

constexpr BYTE versionSignature[12] = { 0x00, 0x56, 0x65, 0x72, 0x73, 0x69, 0x6F, 0x6E, 0x20 ,0x25, 0x73, 0x00 }; // 0x00 Version %s 0x00
BYTE* versionAddress = nullptr;

//typedef int(__cdecl* GP_TakeScreenshot_t)();
//GP_TakeScreenshot_t GP_TakeScreenshot = (GP_TakeScreenshot_t)0x004A5E90;

struct ViewParams {
	int32_t fovMultiplier;	// EDX
	int32_t fogNear;		// ESI
	int32_t fogFar;			// EDX
};

ViewParams viewParams{ 230, 175000, 195000 }; // G-Police Defaults for V_FAR

enum GameState { BRIEFING, COMBAT, CREDITS, DEATH, FLIGHT, HIT, LOADING, LOADOUT, MAP, MENU, MISSION_FAIL, MISSION_SUCCESS, MOVIE, PAUSED, SHUTDOWN, STARTUP };

volatile GameState gameState = GameState::STARTUP;
static GameState previousGameState = (GameState)-1;

constexpr int COMBAT_TIMEOUT = 30;
std::chrono::steady_clock::time_point combatMusicTimer = std::chrono::steady_clock::now();

BYTE* musicVolumeAddress = nullptr;
constexpr BYTE musicVolumeSig[15] = { 0x05, 0x05, 0x00, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x41, 0x42, 0x43 };

/*
75 0C							jne	0x0E
C7 ?? ?? ?? ?? 00 04 00 00 00	mov dword ptr ds:0x??????, 0x04
EB 0F							jmp	0x1D
*/
constexpr BYTE camSig1[14] = { 0x75, 0x0C, 0xC7, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0xEB, 0x0F };
constexpr BYTE camMsk1[14] = { 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
BYTE* camAddress1 = nullptr;

/*
83 FA 05						cmp edx, 0x05
7E 0A							jle 0x0A
C7 ?? ?? ?? ?? 00 01 00 00 00	mov dword ptr ds : 0x??????, 0x01
*/
constexpr BYTE camSig2[15] = { 0x83, 0xFA, 0x05, 0x7E, 0x0A, 0xC7, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00 };
constexpr BYTE camMsk2[15] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
BYTE* camAddress2 = nullptr;

/*
85 DB							test ebx, ebx
7D 0C							jge 0x0C
C7 ?? ?? ?? ?? 00 07 00 00 00	mov dword ptr ds : 0x??????, 0x07
*/
constexpr BYTE camSig3[14] = { 0x85, 0xDB, 0x7D, 0x0C, 0xC7, 0x00, 0x00, 0x00, 0x00, 0x00, 0x07, 0x00, 0x00, 0x00 };
constexpr BYTE camMsk3[14] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
BYTE* camAddress3 = nullptr;

/*
75 0C							jnz 0x0C
C7 ?? ?? ?? ?? 00 05 00 00 00	Mmov dword ptr ds : 0x??????, 0x05
EB 0F							jmp	0x1D
*/
constexpr BYTE camSig4[14] = { 0x75, 0x0C, 0xC7, 0x00, 0x00, 0x00, 0x00, 0x00, 0x05, 0x00, 0x00, 0x00, 0xEB, 0x0F };
constexpr BYTE camMsk4[14] = { 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
BYTE* camAddress4 = nullptr;

/*
83 FB 03						cmp ebx, 0x03
75 0A							jnz 0x0A
C7 ?? ?? ?? ?? 00 02 00 00 00	mov dword ptr ds : 0x??????, 0x02
*/
constexpr BYTE camSig5[15] = { 0x83, 0xFB, 0x03, 0x75, 0x0A, 0xC7, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00 };
constexpr BYTE camMsk5[15] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
BYTE* camAddress5 = nullptr;
BYTE* camAddress6 = nullptr;
BYTE* camAddress7 = nullptr;


/*
EB BE							jmp 0xBE
C7 ?? ?? ?? ?? 00 A8 61 00 00	mov dword ptr ds : 0x??????, 0x61A80000
E8								call
*/
constexpr BYTE LODSig[13] = { 0xEB, 0xBE, 0xC7, 0x00, 0x00, 0x00, 0x00, 0x00, 0xA8, 0x61, 0x00, 0x00, 0xE8 };
constexpr BYTE LODMsk[13] = { 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
BYTE* LODThresholdAddress = nullptr;

/*
6A 28							push 0x40
01 C8							add eax, ecx
*/
constexpr BYTE CamModeClaritySig1[4] = { 0x6A, 0x28, 0x29, 0xC2 };
BYTE* CamModeClarityAddress1 = nullptr;
constexpr BYTE CamModeClaritySig2[4] = { 0x6A, 0x28, 0x01, 0xC8 };
BYTE* CamModeClarityAddress2 = nullptr;

/*
83 3D ?? ?? ?? ?? 00              cmp     ds:0x????????, 0
0F 84 ?? ?? ?? ??                 jz      0x????????
83 EC 08                          sub     esp, 8
D9 05			                  fld
*/
constexpr BYTE InfoCheatSig[18] = { 0x83, 0x3D, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0F, 0x84, 0x00, 0x00, 0x00, 0x00, 0x83, 0xEC, 0x08, 0xD9, 0x05 };
constexpr BYTE InfoCheatMsk[18] = { 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
BYTE* InfoCheatAddress = nullptr;

/*
A0 ?? ?? ?? ??                    mov     al, ds:0x????????
3C 04                             cmp     al, 4
77 BE                             ja      short def_46EE96
25 FF							  and     eax,
*/
constexpr BYTE ViewTweaksSig[11] = { 0xA0, 0x00, 0x00, 0x00, 0x00, 0x3C, 0x04, 0x77, 0x00, 0x25, 0xFF};
constexpr BYTE ViewTweaksMsk[11] = { 0xFF, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0x00, 0xFF, 0xFF };
BYTE* ViewTweaksAddress = nullptr;

/*
8B 15 ?? ?? ?? ??                 mov     edx, ds:0x????????
85 D2                             test    edx, edx
0F 84 F1 01 00 00                 jz      loc_45070C
66 8B ?? ?? ?? ??                 mov     si, word ptr ds:0x????????
00
66 8B							  mov     di,
*/
constexpr BYTE DrawExtraCockpitSig[17] = { 0x8B, 0x15, 0x00, 0x00, 0x00, 0x00, 0x85, 0xD2, 0x0F, 0x84, 0x00, 0x00, 0x00, 0x00, 0x66, 0x8B };
constexpr BYTE DrawExtraCockpitMsk[17] = { 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF };
BYTE* DrawExtraCockpitAddress = nullptr;

/*
AUTO:0044C833 0F 85 ?? ?? ?? ??                 jnz     ????????
AUTO:0044C839 E8 ?? ?? ?? ??                    call    ????????
AUTO:0044C83E A1 ?? ?? ?? ??                    mov     eax, ds:0x????????
AUTO:0044C843 83								cmp
*/
constexpr BYTE NoHUDSig[17] = { 0x0F, 0x85, 0x00, 0x00, 0x00, 0x00, 0xE8, 0x00, 0x00, 0x00, 0x00, 0xA1, 0x00, 0x00, 0x00, 0x00, 0x83 };
constexpr BYTE NoHUDMsk[17] = { 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0xFF, 0x00, 0x00, 0x00, 0x00, 0xFF, 0x00, 0x00, 0x00, 0x00, 0xFF };
BYTE* NoHUDAddress = nullptr;
JumpPatch noHUDJumpPatch = {};

/* Text writing bleeps at briefing/debriefing screen and weapons loadout screen
B9 05 00 00 00                    mov     ecx, 5
C1 FA 1F                          sar     edx, 1Fh
F7 F9                             idiv    ecx
6A 01                             push    1
6A 00                             push    0
68 00 01 00 00                    push    100h
*/
constexpr BYTE BleepFixSig1[19] = { 0xB9, 0x05, 0x00, 0x00, 0x00, 0xC1, 0xFA, 0x1F, 0xF7, 0xF9, 0x6A, 0x01, 0x6A, 0x00, 0x68, 0x00, 0x01, 0x00, 0x00 };

/* End of line beep
B9 05 00 00 00                    mov     ecx, 5
C1 FA 1F                          sar     edx, 1Fh
F7 F9                             idiv    ecx
6A 00                             push    0
6A 00                             push    0
31 DB                             xor     ebx, ebx
68 00 01 00 00                    push    100h
*/
constexpr BYTE BleepFixSig2[21] = { 0xB9, 0x05, 0x00, 0x00, 0x00, 0xC1, 0xFA, 0x1F, 0xF7, 0xF9, 0x6A, 0x00, 0x6A, 0x00, 0x31, 0xDB, 0x68, 0x00, 0x01, 0x00, 0x00 };
BYTE* BleepFixAddress1 = nullptr;
BYTE* BleepFixAddress2 = nullptr;
BYTE* BleepFixAddress3 = nullptr;

/* Cockpit
66 C7 45 CC 80 00                 mov     [ebp+34h], 80h
A1 ?? ?? ?? ??                    mov     eax, ds:0x????????
8B 58 30                          mov     ebx, [eax+30h]
83 C3 3E                          add     ebx, 3Eh ; '>'
*/
constexpr BYTE WireFrameSig1[17] = { 0x66, 0xC7, 0x45, 0xCC, 0x80, 0x00, 0xA1, 0x00, 0x00, 0x00, 0x00, 0x8B, 0x58, 0x30, 0x83, 0xC3, 0x3E };
constexpr BYTE WireFrameMsk1[17] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
BYTE* WireFrameAddress1 = nullptr;

/* Shapes
66 C7 41 38 80 00                 mov     word ptr [ecx+38h], 80h
C7 81 8A 00 00 00                 mov     dword ptr [ecx+8Ah], 0
*/
constexpr BYTE WireFrameSig2[12] = { 0x66, 0xC7, 0x41, 0x38, 0x80, 0x00, 0xC7, 0x81, 0x8A, 0x00, 0x00, 0x00 };
BYTE* WireFrameAddress2 = nullptr;

/* Objects
80 FC 22                          cmp     ah, 22h ; '"'
75 0C                             jnz     short
C7 05 ?? ?? ?? ??                 mov     0x????????, 3E80h
80 3E 00 00
EB 0A                             jmp     short loc_475B3E
C7 05 ?? ?? ?? ??                 mov     0x????????, 7D0h
D0 07 00 00
E8 ?? ?? ?? ??                    call    0x????????
BB 84 00 00 00                    mov     ebx, 84h
*/
constexpr BYTE WireFrameSig3[37] = { 0x80, 0xFC, 0x22, 0x75, 0x0C, 0xC7, 0x05, 0x00, 0x00, 0x00, 0x00, 0x80, 0x3E, 0x00, 0x00, 0xEB, 0x0A, 0xC7, 0x05, 0x00, 0x00, 0x00, 0x00, 0xD0, 0x07, 0x00, 0x00, 0xE8, 0x00, 0x00, 0x00, 0x00, 0xBB, 0x84, 0x00, 0x00, 0x00 };
constexpr BYTE WireFrameMsk3[37] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
BYTE* WireFrameAddress3 = nullptr;

/* Dome
83 EC 0C                          sub     esp, 0Ch
89 C2                             mov     edx, eax
BE 04 04 00 00                    mov     esi, 404h
8B 40 14                          mov     eax, [eax+14h]
30 C9                             xor     cl, cl
30 FF                             xor     bh, bh
*/
constexpr BYTE WireFrameSig4[17] = { 0x83, 0xEC, 0x0C, 0x89, 0xC2, 0xBE, 0x04, 0x04, 0x00, 0x00, 0x8B, 0x40, 0x14, 0x30, 0xC9, 0x30, 0xFF };
BYTE* WireFrameAddress4 = nullptr;

/*
81 C3 94 11 00 00                 add     ebx, 1194h
E8                                call
*/
constexpr BYTE CivSpawnDelaySig[7] = { 0x81, 0xC3, 0x94, 0x11, 0x00, 0x00, 0xE8 };
BYTE* CivSpawnDelayAddress = nullptr;

/*
85 C0                             test    eax, eax
74 06                             jz      short
81 C9 00 00 01 00                 or      ecx, 10000h
31 DB                             xor     ebx, ebx
8B 3D ?? ?? ?? ??                 mov     edi, ds:0x????????
89 1D ?? ?? ?? ??                 mov     0x????????, ebx
81 FF E2 00 00 00                 cmp     edi, 0E2h
*/
constexpr BYTE POVSig[30] = { 0x85, 0xC0, 0x74, 0x06, 0x81, 0xC9, 0x00, 0x00, 0x01, 0x00, 0x31, 0xDB, 0x8B, 0x3D, 0x00, 0x00, 0x00, 0x00, 0x89, 0x1D, 0x00, 0x00, 0x00, 0x00, 0x81, 0xFF, 0xE2, 0x00, 0x00, 0x00 };
constexpr BYTE POVMsk[30] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
BYTE* POVAddress = nullptr;
BYTE* POVFlagsAddress = nullptr;
constexpr int8_t POV_FLAG_SOUTH = 0x04;
constexpr int8_t POV_FLAG_EAST = 0x08;
constexpr int8_t POV_FLAG_NORTH = 0x10;
constexpr int8_t POV_FLAG_WEST = 0x20;
constexpr int8_t POV_DIR_MASK(POV_FLAG_NORTH | POV_FLAG_EAST | POV_FLAG_SOUTH | POV_FLAG_WEST);

/*
E8 ?? ?? ?? ??                    call    0x????????
E8 ?? ?? ?? ??                    call    0x????????
E8 ?? ?? ?? ??                    call    0x????????
E8 ?? ?? ?? ??                    call    0x????????
E8 ?? ?? ?? ??                    call    0x????????
E8 ?? ?? ?? ??                    call    0x????????
A1 ?? ?? ?? ??                    mov     eax, ds:0x????????
E8 ?? ?? ?? ??                    call    0x????????
E8 ?? ?? ?? ??                    call    0x_????????
E8 ?? ?? ?? ??                    call    0x????????
E8 ?? ?? ?? ??                    call    0x????????
E9 ?? ?? ?? ??                    jmp     0x????????
B9 02 00 00 00                    mov     ecx, 2
BB 01 00 00 00                    mov     ebx, 1
BA E0 00 00 00                    mov     edx, 0E0h
*/
constexpr BYTE VirtualCDSig1[75] = { 0xE8, 0x00, 0x00, 0x00, 0x00, 0xE8, 0x00, 0x00, 0x00, 0x00, 0xE8, 0x00, 0x00, 0x00, 0x00, 0xE8, 0x00, 0x00, 0x00, 0x00, 0xE8, 0x00, 0x00, 0x00, 0x00, 0xE8, 0x00, 0x00, 0x00, 0x00, 0xA1, 0x00, 0x00, 0x00, 0x00, 0xE8, 0x00, 0x00, 0x00, 0x00, 0xE8, 0x00, 0x00, 0x00, 0x00, 0xE8, 0x00, 0x00, 0x00, 0x00, 0xE8, 0x00, 0x00, 0x00, 0x00, 0xE9, 0x00, 0x00, 0x00, 0x00,  0xB9, 0x02, 0x00, 0x00, 0x00, 0xBB, 0x01, 0x00, 0x00, 0x00, 0xBA, 0xE0, 0x00, 0x00, 0x00 };
constexpr BYTE VirtualCDMsk1[75] = { 0xFF, 0x00, 0x00, 0x00, 0x00, 0xFF, 0x00, 0x00, 0x00, 0x00, 0xFF, 0x00, 0x00, 0x00, 0x00, 0xFF, 0x00, 0x00, 0x00, 0x00, 0xFF, 0x00, 0x00, 0x00, 0x00, 0xFF, 0x00, 0x00, 0x00, 0x00, 0xFF, 0x00, 0x00, 0x00, 0x00, 0xFF, 0x00, 0x00, 0x00, 0x00, 0xFF, 0x00, 0x00, 0x00, 0x00, 0xFF, 0x00, 0x00, 0x00, 0x00, 0xFF, 0x00, 0x00, 0x00, 0x00, 0xFF, 0x00, 0x00, 0x00, 0x00,  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
BYTE* VirtualCDAddress1 = nullptr;

/*
2E FF 15 F0 83 4F                 call    cs:__imp_MessageBoxA
00
83 F8 02                          cmp     eax, 2
75 CC                             jnz     short
31 C0                             xor     eax, eax
E8 ?? ?? ?? ??                    call    0x????????
EB C3                             jmp     short
E8 ?? ?? ?? ??                    call    0x????????
*/
constexpr BYTE VirtualCDSig2[20] = { 0x00, 0x83, 0xF8, 0x02, 0x75, 0xCC, 0x31, 0xC0, 0xE8, 0x00, 0x00, 0x00, 0x00, 0xEB, 0xC3, 0xE8, 0x00, 0x00, 0x00, 0x00 };
constexpr BYTE VirtualCDMsk2[20] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00 };
BYTE* VirtualCDAddress2 = nullptr;

/*
53                                push    ebx
52                                push    edx
83 EC 04                          sub     esp, 4
8D 44 24 18                       lea     eax, [esp+0Ch+arg_8]
89 E3                             mov     ebx, esp
8B 54 24 14                       mov     edx, [esp+0Ch+arg_4]
89 04 24                          mov     [esp+0Ch+var_C], eax
8B 44 24 10                       mov     eax, [esp+0Ch+arg_0]
E8 15 E3 02 00                    call    vsprintf_
83 C4 04                          add     esp, 4
5A                                pop     edx
5B                                pop     ebx
C3                                retn
*/
constexpr BYTE GPSprintfSig[33] = { 0x53, 0x52, 0x83, 0xEC, 0x04, 0x8D, 0x44, 0x24, 0x18, 0x89, 0xE3, 0x8B, 0x54, 0x24, 0x14, 0x89, 0x04, 0x24, 0x8B, 0x44, 0x24, 0x10, 0xE8, 0x00, 0x00, 0x00, 0x00, 0x83, 0xC4, 0x04, 0x5A, 0x5B, 0xC3 };
constexpr BYTE GPSprintfMsk[33] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
BYTE* GPSprintfAddress = nullptr;
using SprintfFn = int(__cdecl*)(char* dst, const char* fmt, ...);
static SprintfFn SprintfTrampoline = nullptr;
constexpr size_t MAX_INSTRUCTION_TEXT_LENGTH = 512;
static char lastInstruction[MAX_INSTRUCTION_TEXT_LENGTH];

/*
56                                push    esi
57                                push    edi
55                                push    ebp
89 E5                             mov     ebp, esp
83 EC 10                          sub     esp, 10h
89 45 F0                          mov     [ebp+var_10], eax
89 D6                             mov     esi, edx
89 5D F4                          mov     [ebp+var_C], ebx
89 4D F8                          mov     [ebp+var_8], ecx
*/
constexpr BYTE GPDrawHUDTextFuncSig[19] = { 0x56, 0x57, 0x55, 0x89, 0xE5, 0x83, 0xEC, 0x10, 0x89, 0x45, 0xF0, 0x89, 0xD6, 0x89, 0x5D, 0xF4, 0x89, 0x4D, 0xF8 };
BYTE* GPDrawHUDTextFunc = nullptr;

/*
83 EC 04                          sub     esp, 4
BB 00 02 00 00                    mov     ebx, 200h
C1 FA 1F                          sar     edx, 1Fh
F7 FB                             idiv    ebx
83 FA 64                          cmp     edx, 64h
*/
constexpr BYTE SubtitleSignature[18] = { 0x83, 0xEC, 0x04, 0x89, 0xC2, 0xBB, 0x00, 0x02, 0x00, 0x00, 0xC1, 0xFA, 0x1F, 0xF7, 0xFB, 0x83, 0xFA, 0x64 };
BYTE* SubtitleAddress = nullptr;
BYTE* SubtitleTrampoline = nullptr;
/*
A3 ?? ?? ?? ??                    mov     ds:0x????????, eax
FF 15 ?? ?? ?? ??                 call    ds:0x????????
8B 15 ?? ?? ?? ??                 mov     edx, ds:0x????????
31 C0                             xor     eax, eax
FF 15 ?? ?? ?? ??                 call    ds:0x????????
FF 15 ?? ?? ?? ??                 call    ds:????????
85 C0                             test    eax, eax
*/
constexpr BYTE FPSSig[33] = { 0xA3, 0x00, 0x00, 0x00, 0x00, 0xFF, 0x15, 0x00, 0x00, 0x00, 0x00, 0x8B, 0x15, 0x00, 0x00, 0x00, 0x00, 0x31, 0xC0, 0xFF, 0x15, 0x00, 0x00, 0x00, 0x00, 0xFF, 0x15, 0x00, 0x00, 0x00, 0x00, 0x85, 0xC0 };
constexpr BYTE FPSMsk[33] = { 0xFF, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF };
BYTE* FPSAddress = nullptr;
BYTE* FPSTrampoline = nullptr;
BYTE* FPSMOV = nullptr;
LARGE_INTEGER frequency, lastFrameTime, currentTime, elapsedTime;

/*
BE 01 00 00 00                    mov     esi, 1
E8 ?? ?? ?? ??                    call    0x????????
89 35 ?? ?? ?? ??                 mov     ds:0xx????????, esi
8B 8C 24 28 01 00 00              mov     ecx, [esp+128h]
8B 3D ?? ?? ?? ??                 mov     edi, ds:0x????????
80 C9 08                          or      cl, 8
85 FF                             test    edi, edi
74 03                             jz      short +3
80 C9 04                          or      cl, 4
*/
constexpr BYTE PlayMovieSig[39] = { 0xBE, 0x01, 0x00, 0x00, 0x00,  0xE8, 0x00, 0x00, 0x00, 0x00,  0x89, 0x35, 0x00, 0x00, 0x00, 0x00,  0x8B, 0x8C, 0x24, 0x28, 0x01, 0x00, 0x00,  0x8B, 0x3D, 0x00, 0x00, 0x00, 0x00,  0x80, 0xC9, 0x08,  0x85, 0xFF,  0x74, 0x03,  0x80, 0xC9, 0x04 };
constexpr BYTE PlayMovieMsk[39] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,  0xFF, 0x00, 0x00, 0x00, 0x00,  0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00,  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,  0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00,  0xFF, 0xFF, 0xFF,  0xFF, 0xFF,  0xFF, 0xFF,  0xFF, 0xFF, 0xFF };
constexpr DWORD MovieStateValue = static_cast<DWORD>(GameState::MOVIE);
BYTE* PlayMovieAddress = nullptr;
BYTE* PlayMovieTrampoline = nullptr;

/*
BE 05 00 00 00                    mov     esi, 5
29 D1                             sub     ecx, edx
89 35 ?? ?? ?? ??                 mov     ds:0x????????, esi
81 F9 D0 07 00 00                 cmp     ecx, 7D0h
0F 8E ?? ?? ?? ??                 jle     short jle
A1 ?? ?? ?? ??                    mov     eax, ds:0x????????
89 C2                             mov     edx, eax
C1 FA 1F                          sar     edx, 1Fh
2B C2                             sub     eax, edx
D1 F8                             sar     eax, 1
81 E9 D0 07 00 00                 sub     ecx, 7D0h
*/
constexpr BYTE AfterDeathSig[45] = { 0xBE, 0x05, 0x00, 0x00, 0x00, 0x29, 0xD1, 0x89, 0x35, 0x00, 0x00, 0x00, 0x00, 0x81, 0xF9, 0xD0, 0x07, 0x00, 0x00, 0x0F, 0x8E, 0x00, 0x00, 0x00, 0x00, 0xA1, 0x00, 0x00, 0x00, 0x00, 0x89, 0xC2, 0xC1, 0xFA, 0x1F, 0x2B, 0xC2, 0xD1, 0xF8, 0x81, 0xE9, 0xD0, 0x07, 0x00, 0x00 };
constexpr BYTE AfterDeathMsk[45] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0xFF, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
constexpr DWORD DeathStateValue = static_cast<DWORD>(GameState::DEATH);
BYTE* AfterDeathAddress = nullptr;
BYTE* AfterDeathTrampoline = nullptr;

/*
3D FF 00 00 00                    cmp     eax, 0FFh
74 0A                             jz      short jz
A1 ?? ?? ?? ??                    mov     eax, ds:0x????????
A3 ?? ?? ?? ??                    mov     ds:0x????????, eax
83 FA 23                          cmp     edx, 23h
7D 30                             jge     short jz
FE C2                             inc     dl
31 DB                             xor     ebx, ebx
88 15 ?? ?? ?? ??                 mov     ds:0x????????, dl
E8 ?? ?? ?? ??                    call    0x????????
BA 01 00 00 00                    mov     edx, 1
*/
constexpr BYTE MainMenuSig[42] = { 0x3D, 0xFF, 0x00, 0x00, 0x00, 0x74, 0x0A, 0xA1, 0x00, 0x00, 0x00, 0x00, 0xA3, 0x00, 0x00, 0x00, 0x00, 0x83, 0xFA, 0x23, 0x7D, 0x30, 0xFE, 0xC2, 0x31, 0xDB, 0x88, 0x15, 0x00, 0x00, 0x00, 0x00, 0xE8, 0x00, 0x00, 0x00, 0x00, 0xBA, 0x01, 0x00, 0x00, 0x00 };
constexpr BYTE MainMenuMsk[42] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0xFF, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00,  0xFF, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
constexpr DWORD MenuStateValue = static_cast<DWORD>(GameState::MENU);
BYTE* MainMenuAddress = nullptr;
BYTE* MainMenuTrampoline = nullptr;

/*
B9 01 00 00 00                    mov     ecx, 1
8B 15 ?? ?? ?? ??                 mov     edx, ds:0x????????
31 F6                             xor     esi, esi
85 D2                             test    edx, edx
75 2A                             jnz     short jnz
83 3D ?? ?? ?? ?? 00              cmp     ds:0x?????????, 0
75 10                             jnz     short jnz
30 E4                             xor     ah, ah
89 0D ?? ?? ?? ??                 mov     ds:0x????????, ecx
88 25 ?? ?? ?? ??                 mov     ds:0x????????, ah
EB 07                             jmp     short jmp
C6 05 ?? ?? ?? ?? 80              mov     ds:0x????????, 80h
C7 05 ?? ?? ?? ?? 01 00 00 00     mov     ds:0x????????, 1
*/
constexpr BYTE TrainingMenuSig[59] = { 0xB9, 0x01, 0x00, 0x00, 0x00, 0x8B, 0x15, 0x00, 0x00, 0x00, 0x00, 0x31, 0xF6, 0x85, 0xD2, 0x75, 0x2A, 0x83, 0x3D, 0x00, 0x00, 0x00, 0x00, 0x00, 0x75, 0x10, 0x30, 0xE4, 0x89, 0x0D, 0x00, 0x00, 0x00, 0x00, 0x88, 0x25, 0x00, 0x00, 0x00, 0x00, 0xEB, 0x07, 0xC6, 0x05, 0x00, 0x00, 0x00, 0x00, 0x00, 0xC7, 0x05, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00 };
constexpr BYTE TrainingMenuMsk[59] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF };
BYTE* TrainingMenuAddress = nullptr;
BYTE* TrainingMenuTrampoline = nullptr;

/*
BA 0A 00 00 00        mov edx, 0Ah
E8 ?? ?? ?? ??        call 0x????????
BB 04 00 00 00        mov ebx, 4
BA 0A 00 00 00        mov edx, 0Ah
B8 ?? ?? ?? ??        mov eax, offset 0x????????
E8 ?? ?? ?? ??        call 0x????????
BB 02 00 00 00        mov ebx, 2
BA 0A 00 00 00        mov edx, 0Ah
*/
constexpr BYTE LoadoutSig[40] = { 0xBA, 0x0A, 0x00, 0x00, 0x00, 0xE8, 0x00, 0x00, 0x00, 0x00, 0xBB, 0x04, 0x00, 0x00, 0x00, 0xBA, 0x0A, 0x00, 0x00, 0x00, 0xB8, 0x00, 0x00, 0x00, 0x00, 0xE8, 0x00, 0x00, 0x00, 0x00, 0xBB, 0x02, 0x00, 0x00, 0x00, 0xBA, 0x0A, 0x00, 0x00, 0x00 };
constexpr BYTE LoadoutMsk[40] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0xFF, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
constexpr DWORD LoadoutStateValue = static_cast<DWORD>(GameState::LOADOUT);
BYTE* LoadoutAddress = nullptr;
BYTE* LoadoutTrampoline = nullptr;

/*
BA 33 00 00 00        mov edx, 33h
C7 40 32 01 00 00 00  mov dword ptr [eax+32h], 1
89 15 ?? ?? ?? ??     mov ds:????????
C7 40 3E FF FF FF FF  mov dword ptr [eax+3Eh], 0FFFFFFFFh
8B 15 ?? ?? ?? ??     mov edx, ds:0x????????
B9 2D 00 00 00        mov ecx, 2Dh
*/
constexpr BYTE BriefingSig[36] = { 0xBA, 0x33, 0x00, 0x00, 0x00, 0xC7, 0x40, 0x32, 0x01, 0x00, 0x00, 0x00, 0x89, 0x15, 0x00, 0x00, 0x00, 0x00, 0xC7, 0x40, 0x3E, 0xFF, 0xFF, 0xFF, 0xFF, 0x8B, 0x15, 0x00, 0x00, 0x00, 0x00, 0xB9, 0x2D, 0x00, 0x00, 0x00 };
constexpr BYTE BriefingMsk[36] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
constexpr DWORD BriefingStateValue = static_cast<DWORD>(GameState::BRIEFING);
BYTE* BriefingAddress = nullptr;
BYTE* BriefingTrampoline = nullptr;

/*
B9 01 00 00 00        mov ecx, 1
31 D2                 xor edx, edx
E8 6F 93 07 00        call sprintf_
8B 1D ?? ?? ?? ??     mov ebx, ds:????????
83 C4 08              add esp, 8
85 DB                 test ebx, ebx
74 2D                 short jz
68 ?? ?? ?? ??        push 0x????????
68 ?? ?? ?? ??        push 0x????????
E8 ?? ?? ?? ??        call 0x????????
83 C4 08              add esp, 8
*/
constexpr BYTE DebriefSig[43] = { 0xB9, 0x01, 0x00, 0x00, 0x00, 0x31, 0xD2, 0xE8, 0x00, 0x00, 0x00, 0x00, 0x8B, 0x1D, 0x00, 0x00, 0x00, 0x00, 0x83, 0xC4, 0x08, 0x85, 0xDB, 0x74, 0x2D, 0x68, 0x00, 0x00, 0x00, 0x00, 0x68, 0x00, 0x00, 0x00, 0x00, 0xE8, 0x00, 0x00, 0x00, 0x00,  0x83, 0xC4, 0x08 };
constexpr BYTE DebriefMsk[43] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0xFF, 0x00, 0x00, 0x00, 0x00, 0xFF, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF };
BYTE* DebriefAddress = nullptr;
BYTE* DebriefTrampoline = nullptr;

/*
B8 1C 00 00 00                    mov     eax, 1Ch
E8 ?? ?? ?? ??                    call    0x????????
85 C0                             test    eax, eax
75 12                             jnz     short
F6 05 ?? ?? ?? ?? 80              test    byte ptr ds:dword_5052E4, 80h
74 22                             jz      short
83 3D ?? ?? ?? ??                 cmp     ds:0x????????, 0
*/
constexpr BYTE CreditsSig[25] = { 0xB8, 0x1C, 0x00, 0x00, 0x00, 0xE8, 0x00, 0x00, 0x00, 0x00, 0x85, 0xC0, 0x75, 0x12, 0xF6, 0x05, 0x00, 0x00, 0x00, 0x00, 0x80, 0x74, 0x22, 0x83, 0x3D };
constexpr BYTE CreditsMsk[25] = { 0xFF, 0xFF, 0x00, 0x00, 0x00, 0xFF, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
constexpr DWORD CreditsStateValue = static_cast<DWORD>(GameState::CREDITS);
BYTE* CreditsAddress = nullptr;
BYTE* CreditsTrampoline = nullptr;

/*
B8 32 00 00 00        mov eax, 32h
E8 ?? ?? ?? ??        call 0x????????
53                    push ebx
BA 2C 01 00 00        mov edx, 12Ch
31 C9				  xor ecx, ecx
53                    push ebx
89 F0                 mov eax, esi
BB 32 00 00 00        mov ebx, 32h
E8 ?? ?? ?? ??        call 0x????????
83 3D ?? ?? ?? ?? 00  cmp ds:0x????????, 0
*/
constexpr BYTE HitSig[38] = { 0xB8, 0x32, 0x00, 0x00, 0x00, 0xE8, 0x00, 0x00, 0x00, 0x00, 0x53, 0xBA, 0x2C, 0x01, 0x00, 0x00, 0x31, 0xC9, 0x53, 0x89, 0xF0, 0xBB, 0x32, 0x00, 0x00, 0x00, 0xE8, 0x00, 0x00, 0x00, 0x00, 0x83, 0x3D, 0x00, 0x00, 0x00, 0x00 };
constexpr BYTE HitMsk[38] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00 };
constexpr DWORD HitStateValue = static_cast<DWORD>(GameState::HIT);
BYTE* HitAddress = nullptr;
BYTE* HitTrampoline = nullptr;

/*
8B 0D ?? ?? ?? ??        mov ecx, ds:0x????????
83 F9 04                 cmp ecx, 4
75 38                    jnz short
81 FA C8 00 00 00        cmp edx, 0C8h
0F 8E 1B 01 00 00        jle
6A 01                    push 1
68 FF 00 00 00           push 0FFh
B9 FF 00 00 00           mov ecx, 0FFh
B8 ?? ?? ?? ??           mov eax, 0x????????
68 FF 00 00 00           push 0FFh
*/
constexpr BYTE MissionStateLocatorSig[45] = { 0x8B, 0x0D, 0x00, 0x00, 0x00, 0x00, 0x83, 0xF9, 0x04, 0x75, 0x38, 0x81, 0xFA, 0xC8, 0x00, 0x00, 0x00, 0x0F, 0x8E, 0x00, 0x00, 0x00, 0x00, 0x6A, 0x01, 0x68, 0xFF, 0x00, 0x00, 0x00, 0xB9, 0xFF, 0x00, 0x00, 0x00, 0xB8, 0x00, 0x00, 0x00, 0x00, 0x68, 0xFF, 0x00, 0x00, 0x00 };
constexpr BYTE MissionStateLocatorMsk[45] = { 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0xFF, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0x00 };
BYTE* MissionStateLocator = nullptr;
BYTE* MissionStateAddress = 0;

constexpr int CHUNK = 48;
constexpr int PREFIX_LEN = 1;
constexpr int MAX_TEXT = CHUNK - PREFIX_LEN;
constexpr int SUBTITLE_TIMEOUT = 6;
size_t lengthTracker = 0;
std::chrono::steady_clock::time_point subtitleTimer = std::chrono::steady_clock::now();
constexpr char allowedHUDChars[] = {
	' ', '*','+','-','.','/',';',':','<','=','>','?','@',
	'0','1','2','3','4','5','6','7','8','9',
	'A','B','C','D','E','F','G','H','I','J','K','L','M', 'N','O','P','Q','R','S','T','U','V','W','X','Y','Z',
	'a','b','c','d','e','f','g','h','i','j','k','l','m', 'n','o','p','q','r','s','t','u','v','w','x','y','z'
};
constexpr size_t allowedCount = sizeof(allowedHUDChars) / sizeof(allowedHUDChars[0]);

static bool IsHostProcessFocused() {
	HWND fg = GetForegroundWindow();
	if (!fg || !IsWindow(fg)) { return false; }
	
	DWORD fgPID = 0;
	GetWindowThreadProcessId(fg, &fgPID);
	return fgPID == GetCurrentProcessId();
}

static BYTE* AddOffsetIfFound(BYTE* address, size_t offset) { return address ? address + offset : nullptr; }

bool KeyPressed(int vkey) {
	if (!IsHostProcessFocused()) { return false; }
	bool current_state = (GetAsyncKeyState(vkey) & 0x8000) != 0;
	bool previous_state = keyStates.count(vkey) ? keyStates[vkey] : false;
	keyStates[vkey] = current_state;
	return current_state && !previous_state;
}

bool KeyHeld(int vkey) {
	if (!IsHostProcessFocused()) { return false; }
	return (GetAsyncKeyState(vkey) & 0x8000) != 0;
}

void NumpadToPOVHat(void) {
	uint8_t mask = 0;

	if (KeyHeld(VK_NUMPAD8)) { mask |= POV_FLAG_NORTH; }
	if (KeyHeld(VK_NUMPAD6)) { mask |= POV_FLAG_EAST; }
	if (KeyHeld(VK_NUMPAD2)) { mask |= POV_FLAG_SOUTH; }
	if (KeyHeld(VK_NUMPAD4)) { mask |= POV_FLAG_WEST; }
	if (KeyHeld(VK_NUMPAD9)) { mask |= (POV_FLAG_NORTH | POV_FLAG_EAST); }
	if (KeyHeld(VK_NUMPAD7)) { mask |= (POV_FLAG_NORTH | POV_FLAG_WEST); }
	if (KeyHeld(VK_NUMPAD3)) { mask |= (POV_FLAG_SOUTH | POV_FLAG_EAST); }
	if (KeyHeld(VK_NUMPAD1)) { mask |= (POV_FLAG_SOUTH | POV_FLAG_WEST); }
	if (KeyHeld(VK_NUMPAD5)) mask = 0;

	if ((mask & POV_FLAG_NORTH) && (mask & POV_FLAG_SOUTH)) { mask &= ~(POV_FLAG_NORTH | POV_FLAG_SOUTH); }
	if ((mask & POV_FLAG_EAST) && (mask & POV_FLAG_WEST)) { mask &= ~(POV_FLAG_EAST | POV_FLAG_WEST); }

	uint8_t direction = mask & POV_DIR_MASK;

	uint8_t oldFlags = *POVFlagsAddress;
	*POVFlagsAddress = (oldFlags & ~(POV_DIR_MASK)) | direction;
}

__declspec(naked) void Sprintf_Hook() {
	__asm {
		pushad
		mov     edx, [esp + 32 + 8] 		// fmt is at [esp + 32 + 8]
		// Check fmt == "%.36s"
		cmp     byte ptr[edx], 25h
		jne     skip_log
		cmp     byte ptr[edx + 1], 2Eh
		jne     skip_log
		cmp     byte ptr[edx + 2], 33h
		jne     skip_log
		cmp     byte ptr[edx + 3], 36h
		jne     skip_log
		cmp     byte ptr[edx + 4], 73h
		jne     skip_log
		cmp     byte ptr[edx + 5], 00h
		jne     skip_log

		mov     esi, [esp + 32 + 0Ch]		// src is at [esp + 32 + 0Ch]
		test    esi, esi
		jz      skip_log

		mov     edi, OFFSET lastInstruction // dest = lastInstruction	
		xor		ecx, ecx					// copy src -> lastInstruction

		copy_loop:
			cmp     ecx, MAX_INSTRUCTION_TEXT_LENGTH - 1
			jge     done_copy
			mov     al, [esi + ecx]
			mov		[edi + ecx], al
			cmp     al, 0
			je      done_copy
			inc     ecx
			jmp     copy_loop

		done_copy:
			mov     byte ptr[edi + ecx], 0

		skip_log:
			popad
			jmp     SprintfTrampoline
	}
}

extern "C" void LimitFrameRate();
void __declspec(naked) FPSHook() {
	__asm {
		pushad
		pushfd
		mov     edx, [FPSMOV]
		mov     eax, [esp + 32]
		mov     dword ptr[edx], eax
		call    LimitFrameRate
		popfd
		popad
		jmp		[FPSTrampoline]
	}
}

void InstallFPSHook() {
	FPSAddress = (BYTE*)FindSignatureInMemory((BYTE*)moduleBaseAddress, moduleInfo.SizeOfImage, FPSSig, FPSMsk);
	if (FPSAddress) { 
		spdlog::trace("FPS address locator found at 0x{:X}", (uintptr_t)FPSAddress);
		FPSMOV = *reinterpret_cast<BYTE**>(FPSAddress + 0x01);
		if (!FPSMOV) { spdlog::warn("Could not find address for in-game FPS tracking value."); failCount++; }
	}
	else { spdlog::warn("Could not find address for FPS Limiter hook."); failCount++; }
	FPSTrampoline = Trampoline32(FPSAddress, (BYTE*)&FPSHook, 5);
	if (!QueryPerformanceFrequency(&frequency)) { spdlog::warn("Could not query performance frequency for FPS Limiter."); }
	if (!QueryPerformanceCounter(&lastFrameTime)) { spdlog::warn("Could not query performance counter for FPS Limiter."); }
	if (FPSTrampoline) { spdlog::trace("FPS Limiter hook installed successfully."); successCount++; }
	else { failCount++; }
}

typedef long(__stdcall* NtDelayExecution_t)(BOOLEAN Alertable, PLARGE_INTEGER DelayInterval);
static NtDelayExecution_t pNtDelayExecution = nullptr;

inline void PreciseSleep(double seconds) {
	if (!pNtDelayExecution) {
		HMODULE hNtdll = GetModuleHandleA("ntdll.dll");
		if (hNtdll) { pNtDelayExecution = (NtDelayExecution_t)GetProcAddress(hNtdll, "NtDelayExecution"); }
	}
	if (pNtDelayExecution) {
		LARGE_INTEGER interval = {};
		interval.QuadPart = -(LONGLONG)(seconds * 10'000'000.0);
		pNtDelayExecution(FALSE, &interval);
	}
	else { spdlog::error("NtDelayExecution returned 0"); }
}

void LimitFrameRate() {
	static bool wasLimited = false;
	static double smoothedElapsed = 0.0;
	static double invFreq = 1.0 / (double)frequency.QuadPart;
	static double targetFrameTime = 1.0 / FPSLimit;
	static double targetFrameTicks = targetFrameTime * (double)frequency.QuadPart;
	LARGE_INTEGER now;

	QueryPerformanceCounter(&now);

	if (!wasLimited) {
		lastFrameTime = now;
		smoothedElapsed = 0.0;
		wasLimited = true;
		return;
	}

	double elapsed = (now.QuadPart - lastFrameTime.QuadPart) * invFreq;

	double error = elapsed - targetFrameTime;
	if (error < 0.0) { error = -error; }
	double factor = error * 60.0;
	if (factor < 0.05) { factor = 0.05; }
	if (factor > 0.50) { factor = 0.50; }

	smoothedElapsed = smoothedElapsed * (1.0 - factor) + elapsed * factor;

	if (smoothedElapsed < targetFrameTime) {
		double remaining = targetFrameTime - smoothedElapsed;
		if (remaining > 0.002) { PreciseSleep(remaining - 0.001); }
		do {
			QueryPerformanceCounter(&now);
			elapsed = (now.QuadPart - lastFrameTime.QuadPart) * invFreq;
		} while (elapsed < targetFrameTime);
	}

	double finalElapsed = (now.QuadPart - lastFrameTime.QuadPart) * invFreq;
	if (finalElapsed > targetFrameTime * 2.0) {	lastFrameTime = now; }
	else {
		lastFrameTime.QuadPart += (LONGLONG)targetFrameTicks;
		double overshoot = finalElapsed - targetFrameTime;
		if (overshoot > 0.0) { lastFrameTime.QuadPart += (LONGLONG)(overshoot * (double)frequency.QuadPart); }
	}
}

extern "C" void DrawSubtitles();
void __declspec(naked) SubtitleHook() {
	__asm {
		pushad
		pushfd
		call	DrawSubtitles
		popfd
		popad
		jmp		[SubtitleTrampoline]
	}
}

void InstallSubtitleHooks() {
	SubtitleAddress = AddOffsetIfFound((BYTE*)FindSignatureInMemory((BYTE*)moduleBaseAddress, moduleInfo.SizeOfImage, SubtitleSignature), 0x05);
	if (SubtitleAddress) { spdlog::trace("Subtitle hook address 0x{:X}", (uintptr_t)SubtitleAddress); successCount++; }
	else { spdlog::warn("Could not find address for subtitle hook."); failCount++; }

	GPSprintfAddress = (BYTE*)FindSignatureInMemory((BYTE*)moduleBaseAddress, moduleInfo.SizeOfImage, GPSprintfSig, GPSprintfMsk);
	if (GPSprintfAddress) { spdlog::trace("sprintf hook address 0x{:X}", (uintptr_t)GPSprintfAddress); successCount++; }
	else { spdlog::warn("Could not find address for sprintf hook."); failCount++; }
	
	GPDrawHUDTextFunc = (BYTE*)FindSignatureInMemory((BYTE*)moduleBaseAddress, moduleInfo.SizeOfImage, GPDrawHUDTextFuncSig);
	if (GPDrawHUDTextFunc) { spdlog::trace("DrawHUDText hook address 0x{:X}", (uintptr_t)GPDrawHUDTextFunc); successCount++; }
	else { spdlog::warn("Could not find address for DrawHUDText hook."); failCount++; }

	SubtitleTrampoline = Trampoline32((BYTE*)SubtitleAddress, (BYTE*)&SubtitleHook, 5);
	if (!SubtitleTrampoline) { failCount++; }
	else { successCount++; }

	SprintfTrampoline = reinterpret_cast<SprintfFn>(Trampoline32((BYTE*)GPSprintfAddress, (BYTE*)Sprintf_Hook, 5));
	if (!SprintfTrampoline) { failCount++; }
	else { successCount++; }
}

static void __stdcall GP_DrawHUDText( const char* text, int posX, int posY, int red, int green, int blue, char flags) {
	__asm {
		movzx	eax, flags
		push	eax
		movzx	eax, blue
		push	eax
		movzx	eax, green
		push	eax
		mov		eax, text
		mov		edx, posX
		mov		ecx, red
		mov		ebx, posY
		call	GPDrawHUDTextFunc	// callee: retn 0Ch
	}
}

inline bool IsAllowedHUDChar(unsigned char c) {
	for (size_t i = 0; i < allowedCount; i++) {
		if (allowedHUDChars[i] == c) { return true; }
	}
	return false;
}

void DrawSubtitles() {
	if (!lastInstruction) { return;	}

	const char* s = lastInstruction;
	size_t len = strlen(s);

	if (len == 0) { return; }

	if (len == lengthTracker && std::chrono::steady_clock::now() < subtitleTimer) {
		char lineBuf[CHUNK + 1]{};
		lineBuf[0] = '>'; // Prefix
		int lineIndex = 0;
		size_t i = 0;

		while (i < len) {
			while (i < len && s[i] == ' ') { i++; }
			if (i >= len) { break; }

			size_t lineStart = i;
			size_t logicalLen = 0;
			size_t lastSpace = (size_t)-1;

			while (i < len && logicalLen < MAX_TEXT) {
				unsigned char c = (unsigned char)s[i];
				lastSpace = (c == ' ') ? i : lastSpace;
				logicalLen += IsAllowedHUDChar(c);
				i++;
			}

			size_t end;
			if (i == len) { end = len; }
			else if (logicalLen == MAX_TEXT) {
				if (lastSpace != (size_t)-1) {
					end = lastSpace;
					i = lastSpace + 1;
				}
				else {
					end = lineStart + MAX_TEXT;
					i = end;
				}
			}
			else { end = i; }

			size_t outPos = PREFIX_LEN;
			for (size_t p = lineStart; p < end; p++) {
				if (outPos >= CHUNK) { break; }

				unsigned char c = (unsigned char)s[p];
				if (IsAllowedHUDChar(c)) { lineBuf[outPos++] = c; }
			}
			lineBuf[outPos] = '\0';

			if (outPos > PREFIX_LEN) {
				GP_DrawHUDText(lineBuf, 2, 2 + (lineIndex * 22), 0, 255, 0, 1);
				lineIndex++;
			}
		}
	}
	else if (len != lengthTracker) {
		lengthTracker = len;
		subtitleTimer = std::chrono::steady_clock::now() + std::chrono::seconds(SUBTITLE_TIMEOUT);
	}
}

void __declspec(naked) PlayMovieHook() {
	gameState = GameState::MOVIE;
	__asm {
		pushad
		pushfd
		mov		eax, MovieStateValue
		mov		dword ptr [gameState], eax
		popfd
		popad
		jmp		dword ptr [PlayMovieTrampoline]
	}
}

void __declspec(naked) AfterDeathHook() {
	__asm {
		pushad
		pushfd
		mov		eax, DeathStateValue
		mov		dword ptr [gameState], eax
		popfd
		popad
		jmp		dword ptr [AfterDeathTrampoline]
	}
}

void __declspec(naked) MainMenuHook() {
	__asm {
		pushad
		pushfd
		mov		eax, MenuStateValue
		mov		dword ptr [gameState], eax
		popfd
		popad
		jmp		dword ptr [MainMenuTrampoline]
	}
}

void __declspec(naked) TrainingMenuHook() {
	__asm {
		pushad
		pushfd
		mov		eax, MenuStateValue
		mov		dword ptr [gameState], eax
		popfd
		popad
		jmp		dword ptr [TrainingMenuTrampoline]
	}
}

void __declspec(naked) LoadoutHook() {
	__asm {
		pushad
		pushfd
		mov		eax, LoadoutStateValue
		mov		dword ptr [gameState], eax
		popfd
		popad
		jmp		dword ptr [LoadoutTrampoline]
	}
}

void __declspec(naked) BriefingHook() {
	__asm {
		pushad
		pushfd
		mov		eax, BriefingStateValue
		mov		dword ptr [gameState], eax
		popfd
		popad
		jmp		dword ptr [BriefingTrampoline]
	}
}

/*
BYTE* LoadingAddress = (BYTE*)0x00458F63;//00458FB9;
BYTE* LoadingTrampoline = nullptr;
void __declspec(naked) LoadingHook() {
	gameState = GameState::LOADING;
	__asm {
		mov     ebx, 12h//mov     ecx, 3E8h
		jmp		[LoadingTrampoline]
	}
}
*/

void __declspec(naked) DebriefHook() {
	__asm {
		pushad
		pushfd
		mov		eax, BriefingStateValue
		mov		dword ptr [gameState], eax
		popfd
		popad
		jmp		dword ptr [DebriefTrampoline]
	}
}

void __declspec(naked) CreditsHook() {
	__asm {
		pushad
		pushfd
		mov		eax, CreditsStateValue
		mov		dword ptr [gameState], eax
		popfd
		popad
		jmp		dword ptr [CreditsTrampoline]
	}
}

void __declspec(naked) HitHook() {
	__asm {
		pushad
		pushfd
		mov		eax, HitStateValue
		mov		dword ptr[gameState], eax
		popfd
		popad
		jmp		dword ptr[HitTrampoline]
	}
}

void InstallMusicHooks() {
	
	musicVolumeAddress = (BYTE*)FindSignatureInMemory((BYTE*)moduleBaseAddress, moduleInfo.SizeOfImage, musicVolumeSig);
	if (musicVolumeAddress) { spdlog::trace("Music volume address 0x{:X}", (uintptr_t)musicVolumeAddress); }
	else { spdlog::warn("Could not find address for music volume."); failCount++; }

	PlayMovieAddress = (BYTE*)FindSignatureInMemory((BYTE*)moduleBaseAddress, moduleInfo.SizeOfImage, PlayMovieSig, PlayMovieMsk);
	if (PlayMovieAddress) { spdlog::trace("PlayMovie hook address 0x{:X}", (uintptr_t)PlayMovieAddress); }
	else { spdlog::warn("PlayMovie hook address not found."); failCount++; }
	PlayMovieTrampoline  = Trampoline32(PlayMovieAddress, (BYTE*)&PlayMovieHook, 5);
	if (!PlayMovieTrampoline) { failCount++; }
	else { successCount++; }
	
	AfterDeathAddress = (BYTE*)FindSignatureInMemory((BYTE*)moduleBaseAddress, moduleInfo.SizeOfImage, AfterDeathSig, AfterDeathMsk);
	if (AfterDeathAddress) { spdlog::trace("AfterDeath hook address 0x{:X}", (uintptr_t)AfterDeathAddress); }
	else { spdlog::warn("AfterDeath hook address not found."); failCount++; }
	AfterDeathTrampoline = Trampoline32(AfterDeathAddress, (BYTE*)&AfterDeathHook, 5);
	if (!AfterDeathTrampoline) { failCount++; }
	else { successCount++; }

	MainMenuAddress = (BYTE*)FindSignatureInMemory((BYTE*)moduleBaseAddress, moduleInfo.SizeOfImage, MainMenuSig, MainMenuMsk);
	if (MainMenuAddress) { spdlog::trace("Main Menu hook address 0x{:X}", (uintptr_t)MainMenuAddress); }
	else { spdlog::warn("Main Menu hook address not found."); failCount++; }
	MainMenuTrampoline = Trampoline32(MainMenuAddress, (BYTE*)&MainMenuHook, 5);
	if (!MainMenuTrampoline) { failCount++; }
	else { successCount++; }

	TrainingMenuAddress = (BYTE*)FindSignatureInMemory((BYTE*)moduleBaseAddress, moduleInfo.SizeOfImage, TrainingMenuSig, TrainingMenuMsk);
	if (TrainingMenuAddress) { spdlog::trace("Training Menu hook address 0x{:X}", (uintptr_t)TrainingMenuAddress); }
	else { spdlog::warn("Training Menu hook address not found."); failCount++; }
	TrainingMenuTrampoline = Trampoline32(TrainingMenuAddress, (BYTE*)&TrainingMenuHook, 5);
	if (!TrainingMenuTrampoline) { failCount++; }
	else { successCount++; }

	LoadoutAddress = (BYTE*)FindSignatureInMemory((BYTE*)moduleBaseAddress, moduleInfo.SizeOfImage, LoadoutSig, LoadoutMsk);
	if (LoadoutAddress) { spdlog::trace("Loadout hook address 0x{:X}", (uintptr_t)LoadoutAddress); }
	else { spdlog::warn("Loadout hook address not found."); failCount++; }
	LoadoutTrampoline = Trampoline32(LoadoutAddress, (BYTE*)&LoadoutHook, 5);
	if (!LoadoutTrampoline) { failCount++; }
	else { successCount++; }

	BriefingAddress = (BYTE*)FindSignatureInMemory((BYTE*)moduleBaseAddress, moduleInfo.SizeOfImage, BriefingSig, BriefingMsk);
	if (BriefingAddress) { spdlog::trace("Briefing hook address 0x{:X}", (uintptr_t)BriefingAddress); }
	else { spdlog::warn("Briefing hook address not found."); failCount++; }
	BriefingTrampoline = Trampoline32(BriefingAddress, (BYTE*)&BriefingHook, 5);
	if (!BriefingTrampoline) { failCount++; }
	else { successCount++; }

	DebriefAddress = (BYTE*)FindSignatureInMemory((BYTE*)moduleBaseAddress, moduleInfo.SizeOfImage, DebriefSig, DebriefMsk);
	if (DebriefAddress) { spdlog::trace("Debrief hook address 0x{:X}", (uintptr_t)DebriefAddress); }
	else { spdlog::warn("Debrief hook address not found."); failCount++; }
	DebriefTrampoline = Trampoline32(DebriefAddress, (BYTE*)&DebriefHook, 5);
	if (!DebriefTrampoline) { failCount++; }
	else { successCount++; }

	/*
	if (LoadingAddress) { spdlog::trace("Loading hook address 0x{:X}", (uintptr_t)LoadingAddress); }
	else { spdlog::warn("Loading hook address not found."); failCount++; }
	LoadingTrampoline = Trampoline32(LoadingAddress, (BYTE*)&LoadingHook, 5);
	if (!LoadingTrampoline) { failCount++; }
	else { successCount++; }
	*/

	CreditsAddress = (BYTE*)FindSignatureInMemory((BYTE*)moduleBaseAddress, moduleInfo.SizeOfImage, CreditsSig, CreditsMsk);
	if (CreditsAddress) { spdlog::trace("Credits hook address 0x{:X}", (uintptr_t)CreditsAddress); }
	else { spdlog::warn("Credits hook address not found."); failCount++; }
	CreditsTrampoline = Trampoline32(CreditsAddress, (BYTE*)&CreditsHook, 5);
	if (!CreditsTrampoline) { failCount++; }
	else { successCount++; }
	
	HitAddress = (BYTE*)FindSignatureInMemory((BYTE*)moduleBaseAddress, moduleInfo.SizeOfImage, HitSig, HitMsk);
	if (HitAddress) { spdlog::trace("Hit hook address 0x{:X}", (uintptr_t)HitAddress); }
	else { spdlog::warn("Hit hook address not found."); failCount++; }
	HitTrampoline = Trampoline32(HitAddress, (BYTE*)&HitHook, 5);
	if (!HitTrampoline) { failCount++; }
	else { successCount++; }

	MissionStateLocator = (BYTE*)FindSignatureInMemory((BYTE*)moduleBaseAddress, moduleInfo.SizeOfImage, MissionStateLocatorSig, MissionStateLocatorMsk);
	if (MissionStateLocator) { 
		spdlog::trace("Mission state locator address 0x{:X}", (uintptr_t)MissionStateLocator);
		MissionStateAddress = *reinterpret_cast<BYTE**>(MissionStateLocator + 0x02);
		if (MissionStateAddress) { spdlog::trace("Mission state address 0x{:X}", (uintptr_t)MissionStateAddress); }
		else { successCount++; }
	}
	else { spdlog::warn("Mission state locator address not found."); failCount++; }
}

void ManageMusic() {
	int MissionStatus = *reinterpret_cast<int*>(MissionStateAddress);
	GameState nextState = gameState;

	if (MissionStatus == 4) { nextState = GameState::MISSION_SUCCESS; }
	else if (MissionStatus == 5) { nextState = GameState::MISSION_FAIL; }
	else if (gameState == GameState::HIT) {
		if (previousGameState == GameState::FLIGHT || previousGameState == GameState::COMBAT) {
			combatMusicTimer = std::chrono::steady_clock::now() + std::chrono::seconds(COMBAT_TIMEOUT);
			nextState = GameState::COMBAT;
		}
		else { nextState = previousGameState; }
	}
	else if (gameState == GameState::COMBAT) {
		if (previousGameState != GameState::COMBAT) {
			combatMusicTimer = std::chrono::steady_clock::now() + std::chrono::seconds(COMBAT_TIMEOUT);
			nextState = GameState::COMBAT;
		}
		else if (std::chrono::steady_clock::now() > combatMusicTimer) { nextState = GameState::FLIGHT; }
		else { nextState = GameState::COMBAT; }
	}
	else {
		if (MissionStatus == 0 && MissionStatus != 2) { nextState = GameState::FLIGHT; }
	}

	gameState = nextState;

	if (gameState != previousGameState) {
		switch (gameState) {
		case BRIEFING:
			musicPlayer.stop(true);
			break;

		case COMBAT:
			if (useDA) {
				if (previousGameState != GameState::FLIGHT && previousGameState != GameState::COMBAT) {
					musicPlayer.queueDAPlaylist(true, true, false, 8, 11);
					musicPlayer.playNextTrack();
				}
				else { musicPlayer.queueDAPlaylist(true, true, false, 8, 11); }
			}
			else {
				std::vector<std::string> playList = musicPlayer.getWavFilesWithPrefix("COMBAT_");
				if (previousGameState != GameState::FLIGHT && previousGameState != GameState::COMBAT) {
					if (playList.empty()) { musicPlayer.queueWAVPlaylist(true, true); }
					else { musicPlayer.queueWAVPlaylist(playList, true, true); }
					musicPlayer.playNextTrack();
				}
				else {
					if (playList.empty()) { musicPlayer.queueWAVPlaylist(true, true); }
					else { musicPlayer.queueWAVPlaylist(playList, true, true); }
				}
			}
			break;

		case CREDITS:
			if (useDA) {
				musicPlayer.queueDAPlaylist(false, false, true, 3, 3);
				musicPlayer.playNextTrack();
			}
			else { musicPlayer.playNamed("CREDITS", true); }
			break;

		case DEATH:
			if (previousGameState != GameState::MISSION_FAIL) {
				if (useDA) {
					musicPlayer.queueDAPlaylist(false, false, true, 4, 4);
					musicPlayer.playNextTrack();
				}
				else { musicPlayer.playNamed("DEATH", true); }
			}
			break;

		case FLIGHT:
			if (useDA) {
				if (previousGameState != GameState::FLIGHT && previousGameState != GameState::COMBAT) {
					musicPlayer.queueDAPlaylist(true, true, false, 5, 7);
					musicPlayer.playNextTrack();
				}
				else { musicPlayer.queueDAPlaylist(true, true, false, 5, 7); }
			}
			else {
				std::vector<std::string> playList = musicPlayer.getWavFilesWithPrefix("FLIGHT_");
				if (previousGameState != GameState::FLIGHT && previousGameState != GameState::COMBAT) {
					if (playList.empty()) { musicPlayer.queueWAVPlaylist(true, true); }
					else { musicPlayer.queueWAVPlaylist(playList, true, true); }
					musicPlayer.playNextTrack();
				}
				else {
					if (playList.empty()) { musicPlayer.queueWAVPlaylist(true, true); }
					else { musicPlayer.queueWAVPlaylist(playList, true, true); }
				}
			}
			break;

		case LOADING:
			musicPlayer.stop(true);
			break;

		case LOADOUT:
			if (useDA) {
				musicPlayer.queueDAPlaylist(false, false, true, 2, 2);
				musicPlayer.playNextTrack();
			}
			else { musicPlayer.playNamed("MENU", true); }
			break;

		case MAP:
			//musicPlayer.pause();
			break;

		case MENU:
			if (previousGameState != GameState::CREDITS) {
				if (useDA) {
					musicPlayer.queueDAPlaylist(false, false, false, 2, 2);
					musicPlayer.playNextTrack();
				}
				else { musicPlayer.playNamed("MENU", true); }
			}
			break;

		case MISSION_FAIL:
			if (useDA) {
				musicPlayer.queueDAPlaylist(false, false, true, 4, 4);
				musicPlayer.playNextTrack();
			}
			else { musicPlayer.playNamed("FAIL", true); }
			break;

		case MISSION_SUCCESS:
			if (useDA) {
				musicPlayer.queueDAPlaylist(false, false, true, 3, 3);
				musicPlayer.playNextTrack();
			}
			else { musicPlayer.playNamed("SUCCESS", true); }
			break;

		case MOVIE:
			musicPlayer.stop(true);
			break;

		case PAUSED:
			//musicPlayer.pause();
			break;

		case SHUTDOWN:
			musicPlayer.shutdown();
			break;

		case STARTUP:
			spdlog::info("G-Police Directory is {}", musicPlayer.detectGameDirectory());
			if (musicPlayer.detectCDDrive()) { spdlog::info("G-Police CD Found in Drive {}", musicPlayer.getCDDrive()); }
			else { spdlog::warn("No G-Police CD Detected"); }
			musicPlayer.setVolumeAddress(musicVolumeAddress);
			break;
		}
		previousGameState = gameState;
	}
}

void PrintASCIIArt() {
	SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), FOREGROUND_INTENSITY | FOREGROUND_GREEN | FOREGROUND_BLUE);
	std::cout << std::endl;
	std::cout << "             ...:::::::::::::::;  ::::::::::::::::;;..." << std::endl;
	std::cout << "         ..:$$$$XXXXXXX$$$$$XX&&  $$$$$XXXXXXXXXXX$$$$$X$" << std::endl;
	std::cout << "       :Xxxx+$$$$$$$$$$$$XXxx;$$  $xxXXXXXXXXxxxxxxxxxxxXXX$" << std::endl;
	std::cout << "     $$XX$$$$$$$$$$$$$$$$$$$Xx+$  $:xxXX&&$$$$$XXXXxxxxxxxx$$$" << std::endl;
	std::cout << "   .$$$$$$$$X:x$$$x.x             $xxxxx$$$     x.$+;;.;xxxx&$&." << std::endl;
	std::cout << "  :&$$$$$$$XXX.            x::x.  $XXxxx$$$          .&;+xxxxx&&." << std::endl;
	std::cout << " :&&$$$$$$$$:            .&&xx$.  $$XxxxXX$            .+xxxxxx&&" << std::endl;
	std::cout << " ;&&$$$$$XX;              .X$X.   $XxxxxxX$             x++xxxx&&:" << std::endl;
	std::cout << " $&&$$$$$Xx+;:::::::::::.......;; $xxx;:xx$    $;......:+x+xxxx&&$" << std::endl;
	std::cout << " $&&$$$$$$$$$XXXXXXxxxxxxxxx++xx; $$Xx;:xxX    $X+::+xxxxxxxxx+$&$" << std::endl;
	std::cout << " $&&$$$$XXXXXXXXxxxxxxxxxxxxxx$$; X$$xxxxX$    $$XxxX$$$$$&&&$$.$$" << std::endl;
	std::cout << " $&&&&&&&&&&&&&&&&&&&$$$&&xxxx$$; $$$xxxXxX    $&&&&&&&&&&&$$$$$$$" << std::endl;
	std::cout << "                      ;&&xxxxx&&: X&$xxxxx+X" << std::endl;
	std::cout << "                   .$XXXxxxxx$&$  .&&XXXXX+;:X." << std::endl;
	std::cout << "                    X&&xxxxx$$$;   :&&XXXXX&&$" << std::endl;
	std::cout << "                     &&&xx$$$$      :$$xX$&&&" << std::endl;
	std::cout << "                     $$x&&&&          .:x$&&;" << std::endl;
	std::cout << "                      .&x:               .x:" << std::endl;
	std::cout << std::endl;

	SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), FOREGROUND_INTENSITY | FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
}

DWORD __stdcall InjectedThread(HINSTANCE hinstDLL) {
	configValues = ReadConfigFile("Venom.cfg");
	if (!configValues.empty()) {
		showConsole = ReadConfigBool(configValues, "Show_Console", showConsole);
		drawExtraCockpit = ReadConfigBool(configValues, "Draw_Extra_Cockpit", drawExtraCockpit);
		civCarSpawnDelay = ReadConfigNumber(configValues, "Civilian_Vehicle_Spawn_Delay", civCarSpawnDelay);
		enableAllCameraModes = ReadConfigBool(configValues, "Enable_All_Camera_Modes", enableAllCameraModes);
		enableAssetOverride = ReadConfigBool(configValues, "Enable_Asset_Override", enableAssetOverride);
		enablePOVtoKeyboard = ReadConfigBool(configValues, "Enable_POVHat_To_Keyboard", enablePOVtoKeyboard);
		fixBleeps = ReadConfigBool(configValues, "Fix_Bleeps", fixBleeps);
		fixIcon = ReadConfigBool(configValues, "Fix_Icon", fixIcon);
		fixMutex = ReadConfigBool(configValues, "Fix_Mutex", fixMutex);
		fixFocus = ReadConfigBool(configValues, "Fix_Focus", fixFocus);
		FPSLimit = ReadConfigNumber(configValues, "Framerate_Limit", FPSLimit);
		textTweaks = ReadConfigBool(configValues, "Text_Tweaks", textTweaks);
		enableGraphicsProxy = ReadConfigBool(configValues, "Enable_Graphics_Proxy", enableGraphicsProxy);
		enableAudioProxy = ReadConfigBool(configValues, "Enable_Audio_Proxy", enableAudioProxy);
		virtualCD = ReadConfigBool(configValues, "Virtual_CD_Drive", virtualCD);
		enableMediaKeys = ReadConfigBool(configValues, "Enable_Media_Keys", enableMediaKeys);
		subtitlesEnabled = ReadConfigBool(configValues, "Subtitles", subtitlesEnabled);
		terrainLODThreshold = ReadConfigNumber(configValues, "Terrain_LOD_Threshold", terrainLODThreshold);
		useDA = ReadConfigBool(configValues, "Use_CD_Music", useDA);
		viewParams.fovMultiplier = ReadConfigNumber(configValues, "FOV_Multiplier", viewParams.fovMultiplier);
		viewParams.fogNear = ReadConfigNumber(configValues, "Fog_Near", viewParams.fogNear);
		viewParams.fogFar = ReadConfigNumber(configValues, "Fog_Far", viewParams.fogFar);

		if (showConsole) { consoleLogLevel = ReadConfigSpdlogLevel(configValues, "Console_Log_Level", consoleLogLevel); }
		else { consoleLogLevel = spdlog::level::off; }
		fileLogLevel = ReadConfigSpdlogLevel(configValues, "File_Log_Level", fileLogLevel);
		logFilepath = ReadConfigString(configValues, "Log_File_Path", logFilepath);
	}

	if (showConsole) {
		CreateConsole(L"G-Police | Venom.dll");
		PrintASCIIArt();
	}

	if (virtualCD || !useDA) { enableAudioProxy = true; }

	InitSpdlog(logFilepath, fileLogLevel, consoleLogLevel);

	spdlog::info("Venom.dll Build: {} {}", __DATE__, __TIME__);
	spdlog::info("Venom.dll Configuration:");
	spdlog::info(" |- All Camera Modes Enabled: {}", enableAllCameraModes ? "Yes" : "No");
	spdlog::info(" |- Asset Override Enabled: {}", enableAssetOverride ? "Yes" : "No");
	spdlog::info(" |- POV Hat to Keyboard Enabled: {}", enablePOVtoKeyboard ? "Yes" : "No");
	spdlog::info(" |- Civilian Vehicle Spawn Delay: {}", civCarSpawnDelay);
	spdlog::info(" |- Framerate Limit: {} FPS (0 = Unlimited)", FPSLimit);
	spdlog::info(" |- Terrain LOD Threshold: {}", terrainLODThreshold);
	spdlog::info(" |- Text Tweaks Enabled: {}", textTweaks ? "Yes" : "No");
	spdlog::info(" |- Draw Extra Cockpit/Cockpit Fix Enabled: {}", drawExtraCockpit ? "Yes" : "No");
	spdlog::info(" |- Bleep Fix Enabled: {}", fixBleeps ? "Yes" : "No");
	spdlog::info(" |- Focus Fix Enabled: {}", fixFocus ? "Yes" : "No");
	spdlog::info(" |- Icon Fix Enabled: {}", fixIcon ? "Yes" : "No");
	spdlog::info(" |- Mutex Fix Enabled: {}", fixMutex ? "Yes" : "No");
	spdlog::info(" |- Graphics Proxy Enabled: {}", enableGraphicsProxy ? "Yes" : "No");
	spdlog::info(" |- Audio Proxy Enabled: {}", enableAudioProxy ? "Yes" : "No");
	spdlog::info(" |- Media Keys Enabled: {}", enableMediaKeys ? "Yes" : "No");
	spdlog::info(" |- Use CD Music: {}", useDA ? "Yes" : "No");
	spdlog::info(" |- In-Mission Subtitles Enabled: {}", subtitlesEnabled ? "Yes" : "No");
	spdlog::info(" |- Virtual CD Drive Compatibility Enabled: {}", virtualCD ? "Yes" : "No");
	spdlog::info(" |- Console Log Level: {}", spdlog::level::to_string_view(consoleLogLevel));
	spdlog::info(" |- File Log Level: {}", spdlog::level::to_string_view(fileLogLevel));
	spdlog::info(" |- Log File Path: {}", logFilepath.empty() ? "[None specified, file logging disabled]" : logFilepath);

	successCount = 0;
	failCount = 0;

	moduleBaseAddress = GetModuleHandleW(0); // 4194304 (0x00400000)
	K32GetModuleInformation(GetCurrentProcess(), moduleBaseAddress, &moduleInfo, sizeof(MODULEINFO));

	versionAddress = FindNullTerminatedStringInMemory((BYTE*)moduleBaseAddress, moduleInfo.SizeOfImage, versionSignature);
	if (versionAddress) {
		spdlog::trace("G-Police version string found at 0x{:X}.", (uintptr_t)versionAddress);
		std::string versionString = "";
		for (BYTE* ptr = versionAddress; ptr < (BYTE*)moduleBaseAddress + moduleInfo.SizeOfImage && *ptr != 0x00; ptr++) {
			if (std::isprint(*ptr)) { versionString += (char)*ptr; }
			else { versionString += "."; }
		}
		spdlog::info("Your G-Police executable identifies itself as version: {}", versionString);
		if (versionString.find("Demo") != std::string::npos) {
			spdlog::warn("G-Police Demo version detected. Compatibility not guaranteed.");
		}
	}
	else { spdlog::warn("No version string detected. May indicate a problem."); }

	/*
	hKernel32 = GetModuleHandleA("KERNEL32.dll");
	if (hKernel32) {
		HookAPI32(hKernel32, "LoadLibraryA", (void**)&original_LoadLibraryA, (void*)&hooked_LoadLibraryA, 5) ? successCount++ : failCount++;
		HookAPI32(hKernel32, "LoadLibraryW", (void**)&original_LoadLibraryW, (void*)&hooked_LoadLibraryW, 5) ? successCount++ : failCount++;
		HookAPI32(hKernel32, "GetProcAddress", (void**)&original_GetProcAddress, (void*)&hooked_GetProcAddress, 5) ? successCount++ : failCount++;
	}
	else {
		spdlog::error("Failed to get handle for kernel32.dll. Skipping all hooks reliant on this.");
		failCount += 3;
	}
	*/

	if (enableAudioProxy) {
		PatchIATEntry(GetModuleHandleW(0), "WINMM.DLL", "mciSendCommandA", reinterpret_cast<PROC>(&hooked_mciSendCommandA), original_mciSendCommandA) ? successCount++ : failCount++;
		InstallMusicHooks();
	}

	if (enableAssetOverride) {
		PatchIATEntry(GetModuleHandleW(0), "KERNEL32.DLL", "CloseHandle", reinterpret_cast<PROC>(&hooked_CloseHandle), original_CloseHandle) ? successCount++ : failCount++;
		PatchIATEntry(GetModuleHandleW(0), "KERNEL32.DLL", "CreateFileA", reinterpret_cast<PROC>(&hooked_CreateFileA), original_CreateFileA) ? successCount++ : failCount++;
		PatchIATEntry(GetModuleHandleW(0), "KERNEL32.DLL", "ReadFile", reinterpret_cast<PROC>(&hooked_ReadFile), original_ReadFile) ? successCount++ : failCount++;
		PatchIATEntry(GetModuleHandleW(0), "KERNEL32.DLL", "SetFilePointer", reinterpret_cast<PROC>(&hooked_SetFilePointer), original_SetFilePointer) ? successCount++ : failCount++;
	}

	if (enableGraphicsProxy) {
		PatchIATEntry(GetModuleHandleW(0), "DDRAW.DLL", "DirectDrawCreate", reinterpret_cast<PROC>(&hooked_DirectDrawCreate), original_DirectDrawCreate) ? successCount++ : failCount++;
	}

	//PatchIATEntry(GetModuleHandle(0), "USER32.DLL", "CreateWindowExA", reinterpret_cast<PROC>(&hooked_CreateWindowExA), original_CreateWindowExA) ? successCount++ : failCount++;

	if (fixFocus) {
		PatchIATEntry(GetModuleHandleW(0), "DINPUT.DLL", "DirectInputCreateA", reinterpret_cast<PROC>(&hooked_DirectInputCreateA), original_DirectInputCreateA) ? successCount++ : failCount++;
	}

	if (fixIcon) {
		PatchIATEntry(GetModuleHandleW(0), "USER32.DLL", "LoadIconA", reinterpret_cast<PROC>(&hooked_LoadIconA), original_LoadIconA) ? successCount++ : failCount++;
	}

	if (fixMutex) {
		PatchIATEntry(GetModuleHandleW(0), "KERNEL32.DLL", "CreateMutexA", reinterpret_cast<PROC>(&hooked_CreateMutexA), original_CreateMutexA) ? successCount++ : failCount++;
		PatchIATEntry(GetModuleHandleW(0), "KERNEL32.DLL", "OpenMutexA", reinterpret_cast<PROC>(&hooked_OpenMutexA), original_OpenMutexA) ? successCount++ : failCount++;
	}

	if (spdlog::get_level() != spdlog::level::off) {
		PatchIATEntry(GetModuleHandleW(0), "KERNEL32.DLL", "WriteConsoleA", reinterpret_cast<PROC>(&hooked_WriteConsoleA), original_WriteConsoleA) ? successCount++ : failCount++;
		PatchIATEntry(GetModuleHandleW(0), "KERNEL32.DLL", "WriteFile", reinterpret_cast<PROC>(&hooked_WriteFile), original_WriteFile) ? successCount++ : failCount++;
	}

	if (FPSLimit >= 1.0) { InstallFPSHook(); }

	if (subtitlesEnabled) { InstallSubtitleHooks(); }

	if (failCount == 0) { spdlog::info("{}/{} hooks applied successfully.", successCount, successCount + failCount); }
	else { spdlog::error("{}/{} hooks failed.", failCount, successCount + failCount); }


	successCount = 0;
	failCount = 0;

	if (enableAllCameraModes) {
		camAddress1 = AddOffsetIfFound((BYTE*)FindSignatureInMemory((BYTE*)moduleBaseAddress, moduleInfo.SizeOfImage, camSig1, camMsk1), 0x08);
		if (camAddress1) {
			spdlog::trace("Camera mode menu signature 1 found at 0x{:X}", (uintptr_t)camAddress1);
			if (PatchMemory(camAddress1, (BYTE)0x03)) {
				successCount++;
				camAddress6 = AddOffsetIfFound((BYTE*)FindSignatureInMemory((BYTE*)moduleBaseAddress, moduleInfo.SizeOfImage, camSig1, camMsk1), 0x08);
				if (camAddress6) {
					spdlog::trace("Camera mode menu signature 6 found at 0x{:X}", (uintptr_t)camAddress6);
					if (PatchMemory(camAddress6, (BYTE)0x03)) { successCount++; }
					else { failCount++; spdlog::error("Camera mode menu patch 6 failed."); }
				}
				else { spdlog::warn("Could not find address 6 for camera mode menu patch."); }
			}
			else { failCount++; spdlog::error("Camera mode menu patch 1 failed."); }
		}
		else { spdlog::warn("Could not find address 1 for camera mode menu patch."); }

		camAddress2 = AddOffsetIfFound((BYTE*)FindSignatureInMemory((BYTE*)moduleBaseAddress, moduleInfo.SizeOfImage, camSig2, camMsk2), 0x0B);
		if (camAddress2) {
			spdlog::trace("Camera mode menu signature 2 found at 0x{:X}", (uintptr_t)camAddress2);
			if (PatchMemory(camAddress2, (BYTE)0x00)) {
				successCount++;
				camAddress7 = AddOffsetIfFound((BYTE*)FindSignatureInMemory((BYTE*)moduleBaseAddress, moduleInfo.SizeOfImage, camSig2, camMsk2), 0x0B);
				if (camAddress7) {
					spdlog::trace("Camera mode menu signature 7 found at 0x{:X}", (uintptr_t)camAddress7);
					if (PatchMemory(camAddress7, (BYTE)0x00)) { successCount++; }
					else { failCount++; spdlog::error("Camera mode menu patch 7 failed."); }
				}
				else { spdlog::warn("Could not find address 7 for camera mode menu patch."); }
			}
			else { failCount++; spdlog::error("Camera mode menu patch 2 failed."); }
		}
		else { spdlog::warn("Could not find address 2 for camera mode menu patch."); }

		camAddress3 = AddOffsetIfFound((BYTE*)FindSignatureInMemory((BYTE*)moduleBaseAddress, moduleInfo.SizeOfImage, camSig3, camMsk3), 0x0A);
		if (camAddress3) {
			spdlog::trace("Camera mode menu signature 3 found at 0x{:X}", (uintptr_t)camAddress3);
			if (PatchMemory(camAddress3, (BYTE)0x05)) { successCount++; }
			else { failCount++; spdlog::error("Camera mode menu patch 3 failed."); }
		}
		else { spdlog::warn("Could not find address 3 for camera mode menu patch."); }

		camAddress4 = AddOffsetIfFound((BYTE*)FindSignatureInMemory((BYTE*)moduleBaseAddress, moduleInfo.SizeOfImage, camSig4, camMsk4), 0x08);
		if (camAddress4) {
			spdlog::trace("Camera mode menu signature 4 found at 0x{:X}", (uintptr_t)camAddress4);
			if (PatchMemory(camAddress4, (BYTE)0x00)) { successCount++; }
			else { failCount++; spdlog::error("Camera mode menu patch 4 failed."); }
		}
		else { spdlog::warn("Could not find address 4 for camera mode menu patch."); }

		camAddress5 = AddOffsetIfFound((BYTE*)FindSignatureInMemory((BYTE*)moduleBaseAddress, moduleInfo.SizeOfImage, camSig5, camMsk5), 0x0B);
		if (camAddress5) {
			spdlog::trace("Camera mode menu signature 5 found at 0x{:X}", (uintptr_t)camAddress5);
			if (PatchMemory(camAddress5, (BYTE)0x03)) { successCount++; }
			else { failCount++; spdlog::error("Camera mode menu patch 5 failed."); }
		}
		else { spdlog::warn("Could not find address 5 for camera mode menu patch."); }
	}

	if (enablePOVtoKeyboard) {
		POVAddress = AddOffsetIfFound((BYTE*)FindSignatureInMemory((BYTE*)moduleBaseAddress, moduleInfo.SizeOfImage, POVSig, POVMsk), 0x12);
		if (POVAddress) {
			spdlog::trace("POV patch address 0x{:X}", (uintptr_t)POVAddress);
			POVFlagsAddress = reinterpret_cast<BYTE*>(*(uintptr_t*)(POVAddress + 0x02));
			if (POVFlagsAddress) {
				spdlog::trace("POV Flags at 0x{:X}", (uintptr_t)POVFlagsAddress);
				if (NOPMemory(POVAddress, 0x6)) { successCount++; }
				else { failCount++; spdlog::debug("POV Control patch failed."); }
			}
			else { spdlog::warn("Could not find POV flags address."); }
		}
		else { spdlog::warn("Could not find address for POV patch."); }
	}

	if (terrainLODThreshold != 25000 && terrainLODThreshold > 0) {
		LODThresholdAddress = AddOffsetIfFound((BYTE*)FindSignatureInMemory((BYTE*)moduleBaseAddress, moduleInfo.SizeOfImage, LODSig, LODMsk), 0x08);
		if (LODThresholdAddress) {
			spdlog::trace("LOD threshold patch address 0x{:X}. Threshold: {}.", (uintptr_t)LODThresholdAddress, terrainLODThreshold);
			if (PatchMemory(LODThresholdAddress, terrainLODThreshold)) { successCount++; }
			else { failCount++; spdlog::warn("LOD Threshold Patch failed."); }
		}
		else { spdlog::warn("Could not find address for LOD Threshold Patch."); }
	}
	else { spdlog::debug("LOD Threshold of {} -> Patch skipped.", terrainLODThreshold); }

	if (textTweaks) {
		CamModeClarityAddress1 = AddOffsetIfFound((BYTE*)FindSignatureInMemory((BYTE*)moduleBaseAddress, moduleInfo.SizeOfImage, CamModeClaritySig1), 0x01);
		if (CamModeClarityAddress1) {
			spdlog::trace("Pause menu camera mode clarity fix 1 patch address 0x{:X}", (uintptr_t)CamModeClarityAddress1);
			if (PatchMemory(CamModeClarityAddress1, (BYTE)0x0A)) { successCount++; }
			else { failCount++; spdlog::error("Pause menu camera mode clarity fix 1 failed."); }
		}
		else { spdlog::warn("Could not find address for pause menu camera mode clarity fix 1."); }

		CamModeClarityAddress2 = AddOffsetIfFound((BYTE*)FindSignatureInMemory((BYTE*)moduleBaseAddress, moduleInfo.SizeOfImage, CamModeClaritySig2), 0x01);
		if (CamModeClarityAddress2) {
			spdlog::trace("Pause menu camera mode clarity fix 2 patch address 0x{:X}", (uintptr_t)CamModeClarityAddress2);
			if (PatchMemory(CamModeClarityAddress2, (BYTE)0x0A)) { successCount++; }
			else { failCount++; spdlog::error("Pause menu camera mode clarity fix 2 failed."); }
		}
		else { spdlog::warn("Could not find address for pause menu camera mode clarity fix 2."); }
	}

	if (viewParams.fovMultiplier != 230 || viewParams.fogNear != 175000 || viewParams.fogFar != 195000) {
		ViewTweaksAddress = (BYTE*)FindSignatureInMemory((BYTE*)moduleBaseAddress, moduleInfo.SizeOfImage, ViewTweaksSig, ViewTweaksMsk);
		if (ViewTweaksAddress) {
			spdlog::trace("View Tweaks patch address 0x{:X}", (uintptr_t)ViewTweaksAddress);

			if (PatchMemory(ViewTweaksAddress, { 0xB0, 0x04, 0x90, 0x90, 0x90 })) { successCount++; }
			else { failCount++; spdlog::warn("Failed to force V_FAR path (requied for view tweaks)."); }

			BYTE viewPatch1[6] = {
				0x69, 0xD6,
				(BYTE)(viewParams.fovMultiplier),
				(BYTE)(viewParams.fovMultiplier >> 8),
				(BYTE)(viewParams.fovMultiplier >> 16),
				(BYTE)(viewParams.fovMultiplier >> 24)
			};
			if (PatchMemory((BYTE*)ViewTweaksAddress - 0xF4, viewPatch1)) { successCount++; }
			else { failCount++; spdlog::warn("View patch 1 failed."); }

			BYTE viewPatch2[5] = {
				0xBE,
				(BYTE)(viewParams.fogNear),
				(BYTE)(viewParams.fogNear >> 8),
				(BYTE)(viewParams.fogNear >> 16),
				(BYTE)(viewParams.fogNear >> 24)
			};
			if (PatchMemory(ViewTweaksAddress - 0xE2, viewPatch2)) { successCount++; }
			else { failCount++; spdlog::warn("View patch 2 failed."); }

			BYTE viewPatch3[5] = {
				0xBA,
				(BYTE)(viewParams.fogFar),
				(BYTE)(viewParams.fogFar >> 8),
				(BYTE)(viewParams.fogFar >> 16),
				(BYTE)(viewParams.fogFar >> 24)
			};
			if (PatchMemory(ViewTweaksAddress - 0xDD, viewPatch3)) { successCount++; }
			else { failCount++; spdlog::warn("View patch 3 failed."); }
		}
		else { spdlog::warn("Could not find address for view tweaks."); }
	}

	if (drawExtraCockpit) {
		DrawExtraCockpitAddress = (BYTE*)FindSignatureInMemory((BYTE*)moduleBaseAddress, moduleInfo.SizeOfImage, DrawExtraCockpitSig, DrawExtraCockpitMsk);
		if (DrawExtraCockpitAddress) {
			spdlog::trace("Draw Extra Cockpit patch address 0x{:X}", (uintptr_t)DrawExtraCockpitAddress);
			if (PatchMemory(DrawExtraCockpitAddress, { 0xBA, 0x00, 0x00, 0x00, 0x00, 0x90 })) { successCount++; }
			else { failCount++; spdlog::warn("Draw Extra Cockpit patch failed."); }
		}
		else { spdlog::warn("Could not find address for draw extra cockpit patch."); }
	}
	
	if (fixBleeps) {
		BleepFixAddress1 = AddOffsetIfFound((BYTE*)FindSignatureInMemory((BYTE*)moduleBaseAddress, moduleInfo.SizeOfImage, BleepFixSig1), 0x01);
		if (BleepFixAddress1) {
			spdlog::trace("Bleep Fix 1 patch address 0x{:X}", (uintptr_t)BleepFixAddress1);
			if (PatchMemory(BleepFixAddress1, (BYTE)0x03)) {
				successCount++;
				BleepFixAddress2 = AddOffsetIfFound((BYTE*)FindSignatureInMemory((BYTE*)moduleBaseAddress, moduleInfo.SizeOfImage, BleepFixSig1), 0x01);
				if (BleepFixAddress2) {
					spdlog::trace("Bleep Fix 2 patch address 0x{:X}", (uintptr_t)BleepFixAddress2);
					if (PatchMemory(BleepFixAddress2, (BYTE)0x03)) { successCount++; }
					else { failCount++; spdlog::warn("Bleep Fix patch 2 failed. Bleep Fix patch 2 will be skipped."); }
				}
				else { spdlog::warn("Could not find address for bleep fix 2."); }
			}
			else { failCount++; spdlog::warn("Bleep Fix patch 1 failed. Bleep Fix patch 2 will be skipped."); }
		}
		else { spdlog::warn("Could not find address for bleep fix 1. Bleep fix 2 will also be skipped."); }
		BleepFixAddress3 = AddOffsetIfFound((BYTE*)FindSignatureInMemory((BYTE*)moduleBaseAddress, moduleInfo.SizeOfImage, BleepFixSig2), 0x01);
		if (BleepFixAddress3) {
			spdlog::trace("Bleep Fix 3 patch address 0x{:X}", (uintptr_t)BleepFixAddress3);
			if (PatchMemory(BleepFixAddress3, (BYTE)0x03)) { successCount++; }
			else { failCount++; spdlog::warn("Bleep Fix patch 3 failed."); }
		}
		else { spdlog::warn("Could not find address for bleep fix 3."); }
	}

	if (civCarSpawnDelay != 4500 && civCarSpawnDelay >= 0) {
		CivSpawnDelayAddress = AddOffsetIfFound((BYTE*)FindSignatureInMemory((BYTE*)moduleBaseAddress, moduleInfo.SizeOfImage, CivSpawnDelaySig), 0x02);
		if (CivSpawnDelayAddress) {
			spdlog::trace("Civilian vehicle spawn delay patch address 0x{:X}. Delay: {}.", (uintptr_t)CivSpawnDelayAddress, civCarSpawnDelay);
			if (PatchMemory(CivSpawnDelayAddress, civCarSpawnDelay)) { successCount++; }
			else { failCount++; spdlog::warn("Civilian vehicle spawn delay patch failed."); }
		}
		else { spdlog::warn("Could not find address for civilian vehicle spawn delay patch."); }
	}
	else { spdlog::debug("Civilian vehicle spawn delay of {} -> Patch skipped.", terrainLODThreshold); }

	if (virtualCD) {
		VirtualCDAddress1 = (BYTE*)FindSignatureInMemory((BYTE*)moduleBaseAddress, moduleInfo.SizeOfImage, VirtualCDSig1, VirtualCDMsk1);
		if (VirtualCDAddress1) {
			spdlog::trace("Virtual CD drive compatability patch address 1 0x{:X}", (uintptr_t)VirtualCDAddress1);
			if (NOPMemory(VirtualCDAddress1, 5)) { successCount++; }
			else { failCount++; spdlog::warn("Virtual CD drive compatability patch 1 failed."); }
		}
		else { spdlog::warn("Could not find address for virtual CD drive compatability patch 1."); }
		VirtualCDAddress2 = AddOffsetIfFound((BYTE*)FindSignatureInMemory((BYTE*)moduleBaseAddress, moduleInfo.SizeOfImage, VirtualCDSig2, VirtualCDMsk2), 0x0F);
		if (VirtualCDAddress2) {
			spdlog::trace("Virtual CD drive compatability patch address 2 0x{:X}", (uintptr_t)VirtualCDAddress2);
			if (NOPMemory(VirtualCDAddress2, 5)) { successCount++; }
			else { failCount++; spdlog::warn("Virtual CD drive compatability patch 2 failed."); }
		}
		else { spdlog::warn("Could not find address for virtual CD drive compatability patch 2."); }
	}
		
	InfoCheatAddress = AddOffsetIfFound((BYTE*)FindSignatureInMemory((BYTE*)moduleBaseAddress, moduleInfo.SizeOfImage, InfoCheatSig, InfoCheatMsk), 0x06);
	if (InfoCheatAddress) {	spdlog::trace("InfoCheat patch address 0x{:X}", (uintptr_t)InfoCheatAddress); }
	else { spdlog::warn("Could not find address for info cheat toggle patch."); }
	
	NoHUDAddress = (BYTE*)FindSignatureInMemory((BYTE*)moduleBaseAddress, moduleInfo.SizeOfImage, NoHUDSig, NoHUDMsk);
	if (NoHUDAddress) {	spdlog::trace("NoHUD patch address 0x{:X}", (uintptr_t)NoHUDAddress); }
	else { spdlog::warn("Could not find address for NoHUD toggle patch."); }

	WireFrameAddress1 = AddOffsetIfFound((BYTE*)FindSignatureInMemory((BYTE*)moduleBaseAddress, moduleInfo.SizeOfImage, WireFrameSig1, WireFrameMsk1), 0x04);
	if (WireFrameAddress1) { spdlog::trace("Psuedo-Wireframe signature 1 found at 0x{:X}", (uintptr_t)WireFrameAddress1); }
	else { spdlog::warn("Could not find address 1 for Psuedo-Wireframe toggle patch."); }
	
	WireFrameAddress2 = AddOffsetIfFound((BYTE*)FindSignatureInMemory((BYTE*)moduleBaseAddress, moduleInfo.SizeOfImage, WireFrameSig2), 0x04);
	if (WireFrameAddress2) { spdlog::trace("Psuedo-Wireframe signature 2 found at 0x{:X}", (uintptr_t)WireFrameAddress2); }
	else { spdlog::warn("Could not find address 2 for Psuedo-Wireframe toggle patch."); }

	WireFrameAddress3 = AddOffsetIfFound((BYTE*)FindSignatureInMemory((BYTE*)moduleBaseAddress, moduleInfo.SizeOfImage, WireFrameSig3, WireFrameMsk3), 0x21);
	if (WireFrameAddress3) { spdlog::trace("Psuedo-Wireframe signature 3 found at 0x{:X}", (uintptr_t)WireFrameAddress3); }
	else { spdlog::warn("Could not find address 3 for Psuedo-Wireframe toggle patch."); }

	WireFrameAddress4 = AddOffsetIfFound((BYTE*)FindSignatureInMemory((BYTE*)moduleBaseAddress, moduleInfo.SizeOfImage, WireFrameSig4), 0x06);
	if (WireFrameAddress4) { spdlog::trace("Psuedo-Wireframe signature 4 found at 0x{:X}", (uintptr_t)WireFrameAddress4); }
	else { spdlog::warn("Could not find address 4 for Psuedo-Wireframe toggle patch."); }
	
	if (failCount == 0) { spdlog::info("{}/{} memory patches applied successfully.", successCount, successCount + failCount); }
	else { spdlog::error("{}/{} memory patches failed.", failCount, successCount + failCount); }

	while (!KeyPressed(VK_END)) {
		if (KeyPressed(VK_F2)) {
			if (InfoCheatAddress) { PatchMemory(InfoCheatAddress, (uint8_t)(*(uint8_t*)InfoCheatAddress ^ 1)); }
		}
				
		
		if (KeyPressed(VK_F3)) {
			if (NoHUDAddress) {
				ToggleCondJMPToJMP(NoHUDAddress, noHUDJumpPatch, !noHUD);
				noHUD = !noHUD;
			}
		}

		if (KeyPressed(VK_F4)) {
			if (psuedoWireframe) { 
				if (WireFrameAddress1) { PatchMemory(WireFrameAddress1, (BYTE)0x80); }
				if (WireFrameAddress2) { PatchMemory(WireFrameAddress2, (BYTE)0x80); }
				if (WireFrameAddress3) { PatchMemory(WireFrameAddress3, (BYTE)0x84); }
				if (WireFrameAddress4) { PatchMemory(WireFrameAddress4, { 0x04, 0x04 }); }
				psuedoWireframe = false;
			}
			else { 
				if (WireFrameAddress1) { PatchMemory(WireFrameAddress1, (BYTE)0x02); }
				if (WireFrameAddress2) { PatchMemory(WireFrameAddress2, (BYTE)0x01); }
				if (WireFrameAddress3) { PatchMemory(WireFrameAddress3, (BYTE)0x01); }
				if (WireFrameAddress4) { PatchMemory(WireFrameAddress4, { 0x01, 0x00 }); }
				psuedoWireframe = true;
			}
		}
		
		if (enablePOVtoKeyboard && POVFlagsAddress) { NumpadToPOVHat(); }

		// if (KeyPressed(VK_SNAPSHOT)) { GP_TakeScreenshot(); }

		if (enableAudioProxy) {
			ManageMusic();
			if (enableMediaKeys && enableAudioProxy) {
				if (KeyPressed(VK_MEDIA_PLAY_PAUSE)) {
					if (!musicPlayer.isPlaying()) {
						if (musicPlayer.isPaused()) { musicPlayer.resume(); }
						else { musicPlayer.playNextTrack(); }
					}
					else { musicPlayer.pause(); }
				}

				if (KeyPressed(VK_MEDIA_NEXT_TRACK)) { musicPlayer.playNextTrack(); }
				if (KeyPressed(VK_MEDIA_PREV_TRACK)) { musicPlayer.playPreviousTrack(); }
				if (KeyPressed(VK_MEDIA_STOP)) { musicPlayer.stop(false);}
			}
			musicPlayer.update();
		}
		Sleep(20);
	}

	spdlog::info("END key pressed. Venom.dll will be unloaded.");
	musicPlayer.shutdown();
	if (showConsole) { FreeConsole(); }
	spdlog::shutdown();
	if (threadHandle) {
		CloseHandle(threadHandle);
		threadHandle = nullptr;
	}
	FreeLibraryAndExitThread(hinstDLL, 0);
	return 0;
}

BOOL __stdcall DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved) {
	switch (fdwReason) {
	case DLL_PROCESS_ATTACH:
		DisableThreadLibraryCalls(hinstDLL);
		threadHandle = CreateThread(0, 0, (LPTHREAD_START_ROUTINE)InjectedThread, hinstDLL, 0, 0);
		break;
	case DLL_THREAD_ATTACH:
		break;
	case DLL_THREAD_DETACH:
		break;
	case DLL_PROCESS_DETACH:
		break;
	}
	return true;
}
