// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once
#define WIN32_LEAN_AND_MEAN
#include <algorithm>
#include <cctype>
#include <string>
#include <unordered_map>
#include <exception>
#include <cstdint>
#include <type_traits>

std::string Trim(const std::string &str);
bool ReadConfigBool(const std::unordered_map<std::string, std::string> &config, const std::string &key, bool defaultValue);
std::string ReadConfigString(const std::unordered_map<std::string, std::string> &config, const std::string &key, std::string defaultValue);
std::unordered_map<std::string, std::string> ReadConfigFile(const std::string &filename);

template <typename T>
T ReadConfigNumber(const std::unordered_map<std::string, std::string> &config, const std::string &key, T defaultValue) {
	std::string lowerKey = key;
	std::transform(lowerKey.begin(), lowerKey.end(), lowerKey.begin(), ::tolower);

	auto it = config.find(lowerKey);
	if (it == config.end()) { return defaultValue; }

	const std::string& value = it->second;

	try {
		if constexpr (std::is_same_v<T, int> ||
			std::is_same_v<T, int32_t> ||
			std::is_same_v<T, uint32_t> ||
			std::is_same_v<T, long>) {
			return static_cast<T>(std::stoi(value));
		}
		else if constexpr (std::is_same_v<T, float>) { return std::stof(value); }
		else if constexpr (std::is_same_v<T, double>) { return std::stod(value); }
		else { static_assert(!sizeof(T), "Unsupported type for ReadNumber"); }
	}
	catch (const std::exception&) { return defaultValue; }
}