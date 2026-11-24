// SPDX-License-Identifier: GPL-3.0-or-later
// spdlog 1.17.0 (https://github.com/gabime/spdlog/) is licenced under the MIT License, which is GPL-3.0 compatible.

#pragma once
#define WIN32_LEAN_AND_MEAN
#include <ddraw.h>
#include <mmsystem.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

#pragma comment(lib, "winmm.lib")

#ifndef MCI_GETDEVCAPS_TIME_FORMAT
#define MCI_GETDEVCAPS_TIME_FORMAT 0x00000004
#endif


void CreateConsole(std::wstring title);
void InitSpdlog(const std::string& fileName, spdlog::level::level_enum fileLevel, spdlog::level::level_enum consoleLevel);
void DumpBytes(const BYTE* address, int length);
UINT GetMCITimeFormat(MCIDEVICEID deviceId);
void LogLastError(DWORD err);
void LogLastError();
void LogMCICommand(MCIDEVICEID IDDevice, UINT uMsg, DWORD_PTR fdwCommand, DWORD_PTR dwParam);
void LogMCIResult(MCIERROR err);
void LogMCITimeValue(UINT timeFormat, DWORD value, const char* label);
void LogSurfaceDetails(const char* functionName, DWORD flags, LPDDCOLORKEY lpDDColorKey, LPDDSURFACEDESC lpDDSurfaceDesc);
std::string ReadConfigString(const std::unordered_map<std::string, std::string>& config, const std::string& key, std::string defaultValue);
spdlog::level::level_enum ReadConfigSpdlogLevel(const std::unordered_map<std::string, std::string>& config, const std::string& key, spdlog::level::level_enum defaultValue);
std::string REFIIDToString(REFIID riid);
std::string WideStringToString(LPCWSTR wide_string);
