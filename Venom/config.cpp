// SPDX-License-Identifier: GPL-3.0-or-later

#include "config.h"
#include <string>
#include <algorithm>
#include <cctype>
#include <fstream>
#include <unordered_map>
#include <spdlog/common.h>

std::string Trim(const std::string &str) {
	size_t first = str.find_first_not_of(" \t\n\r");
	if (std::string::npos == first) { return str; }
	size_t last = str.find_last_not_of(" \t\n\r");
	return str.substr(first, (last - first + 1));
}

bool ReadConfigBool(const std::unordered_map<std::string, std::string>& config, const std::string &key, bool defaultValue) {
	std::string lowerKey = key;
	std::transform(lowerKey.begin(), lowerKey.end(), lowerKey.begin(), ::tolower);
	if (config.count(lowerKey)) {
		const std::string &value = config.at(lowerKey);
		std::string lowerValue = value;
		std::transform(lowerValue.begin(), lowerValue.end(), lowerValue.begin(), ::tolower);
		if (lowerValue == "true" || lowerValue == "1" || lowerValue == "yes") { return true; }
		if (lowerValue == "false" || lowerValue == "0" || lowerValue == "no") { return false; }
	}
	return defaultValue;
}

std::string ReadConfigString(const std::unordered_map<std::string, std::string> &config, const std::string &key, std::string defaultValue) {
	std::string lowerKey = key;
	std::transform(lowerKey.begin(), lowerKey.end(), lowerKey.begin(), ::tolower);

	if (config.count(lowerKey)) {
		const std::string &value = config.at(lowerKey);
		std::string lowerValue = value;
		std::transform(lowerValue.begin(), lowerValue.end(), lowerValue.begin(), ::tolower);

		if (lowerValue == "" || lowerValue == "empty" || lowerValue == "none" || lowerValue == "0") { return ""; }
		return value;
	}
	return defaultValue;
}

spdlog::level::level_enum ReadConfigSpdlogLevel(const std::unordered_map<std::string, std::string> &config, const std::string &key, spdlog::level::level_enum defaultValue) {
	std::string levelString = ReadConfigString(config, key, "");
	if (!levelString.empty()) {
		std::transform(levelString.begin(), levelString.end(), levelString.begin(), [](unsigned char c) { return std::tolower(c); });
		if (levelString == "trace" || levelString == "all") { return spdlog::level::trace; }
		if (levelString == "debug" || levelString == "dbg") { return spdlog::level::debug; }
		if (levelString == "info" || levelString == "information") { return spdlog::level::info; }
		if (levelString == "warn" || levelString == "warning") { return spdlog::level::warn; }
		if (levelString == "err" || levelString == "error") { return spdlog::level::err; }
		if (levelString == "critical") { return spdlog::level::critical; }
		if (levelString == "off" || levelString == "none" || levelString == "disabled" || levelString == "0") { return spdlog::level::off; }
	}
	return defaultValue;
}

std::unordered_map<std::string, std::string> ReadConfigFile(const std::string &fileName) {
	std::unordered_map<std::string, std::string> config;
	std::ifstream file(fileName);
	std::string line;
	std::string currentSection;
	size_t lineCount = 0;

	if (file.is_open()) {
		while (std::getline(file, line)) {
			lineCount++;
			size_t commentPos = line.find_first_of(";#");
			if (commentPos != std::string::npos) { line = line.substr(0, commentPos); }

			std::string trimmedLine = Trim(line);

			if (trimmedLine.empty()) { continue; }

			if (trimmedLine[0] == '[' && trimmedLine.back() == ']') { currentSection = Trim(trimmedLine.substr(1, trimmedLine.length() - 2)); }
			else {
				size_t equalsPos = trimmedLine.find('=');
				if (equalsPos != std::string::npos) {
					std::string key = Trim(trimmedLine.substr(0, equalsPos));
					std::string value = Trim(trimmedLine.substr(equalsPos + 1));

					if (!key.empty()) {
						std::transform(key.begin(), key.end(), key.begin(), ::tolower);
						config[key] = value;
					}
				}
			}
		}
		file.close();
	}
	return config;
}