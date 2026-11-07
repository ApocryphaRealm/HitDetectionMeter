#include "PCH.h"

#include "Settings.h"

#include "utils/Logger.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <format>
#include <fstream>
#include <vector>

namespace settings
{
	namespace
	{
		std::string iniPath;

		std::string Lower(std::string a_s)
		{
			for (char& c : a_s) { c = static_cast<char>(std::tolower(static_cast<unsigned char>(c))); }
			return a_s;
		}

		std::string Trim(const std::string& a_s)
		{
			const auto b = a_s.find_first_not_of(" \t\r\n");
			if (b == std::string::npos) { return {}; }
			const auto e = a_s.find_last_not_of(" \t\r\n");
			return a_s.substr(b, e - b + 1);
		}

		bool Flag(const std::string& a_v) { return Lower(a_v) == "1" || Lower(a_v) == "true"; }

		bool LoadFileValues()
		{
			std::ifstream in(iniPath);
			if (!in)
			{
				logger::warn("INI not found at {}; keeping the current values", iniPath);
				return false;
			}
			std::string line, section;
			while (std::getline(in, line))
			{
				const std::string t = Trim(line);
				if (t.empty() || t[0] == ';' || t[0] == '#') { continue; }
				if (t.front() == '[' && t.back() == ']') { section = Lower(t.substr(1, t.size() - 2)); continue; }
				const auto eq = t.find('=');
				if (eq == std::string::npos) { continue; }
				const std::string key = Lower(Trim(t.substr(0, eq)));
				const std::string value = Trim(t.substr(eq + 1));
				try
				{
					if (section == "general")
					{
						if (key == "benabled") { enabled = Flag(value); }
						else if (key == "bshowfriendlyhits") { showFriendlyHits = Flag(value); }
						else if (key == "fduration") { duration = std::clamp(std::stof(value), 0.2F, 10.0F); }
						else if (key == "fsize") { size = std::clamp(std::stof(value), 0.25F, 4.0F); }
						else if (key == "fopacity") { opacity = std::clamp(std::stof(value), 0.0F, 1.0F); }
						else if (key == "icolorr") { colorR = std::clamp(std::stoi(value), 0, 255); }
						else if (key == "icolorg") { colorG = std::clamp(std::stoi(value), 0, 255); }
						else if (key == "icolorb") { colorB = std::clamp(std::stoi(value), 0, 255); }
						else if (key == "imaxmarkers") { maxMarkers = std::clamp(std::stoi(value), 1, 32); }
					}
					else if (section == "ring")
					{
						if (key == "fringoffset") { ringOffset = std::clamp(std::stof(value), -300.0F, 600.0F); }
						else if (key == "fradius") { radius = std::clamp(std::stof(value), 0.0F, 2000.0F); }
						else if (key == "fcenteroffsetx") { centerOffsetX = std::clamp(std::stof(value), -1000.0F, 1000.0F); }
						else if (key == "fcenteroffsety") { centerOffsetY = std::clamp(std::stof(value), -1000.0F, 1000.0F); }
					}
					else if (section == "debug" && key == "uloglevel") { logLevel = static_cast<std::uint32_t>(std::stoul(value, nullptr, 0)); }
				}
				catch (...)
				{
					logger::warn("INI value \"{}\" for {} is not valid; keeping the current value", value, key);
				}
			}
			logger::info("settings read from {}: enabled={} friendly={} duration={:.2f}s size={:.2f} opacity={:.2f} colour={},{},{} maxMarkers={} logLevel={}",
				iniPath, enabled.load(), showFriendlyHits.load(), duration.load(), size.load(), opacity.load(), colorR.load(), colorG.load(), colorB.load(),
				maxMarkers.load(), logLevel.load());
			logger::info("ring: offset {:.0f} px; without Detection Meter: radius {:.0f}, centre offset {:.0f},{:.0f}", ringOffset.load(), radius.load(),
				centerOffsetX.load(), centerOffsetY.load());
			return true;
		}

		// Replaces the key's value in place, so comments survive; adds the key when it is missing.
		void WriteKey(std::vector<std::string>& a_lines, const char* a_section, const char* a_key, const std::string& a_value)
		{
			const std::string wantSection = Lower(a_section);
			const std::string wantKey = Lower(a_key);
			std::string section;
			std::size_t lastInSection = std::string::npos;
			for (std::size_t i = 0; i < a_lines.size(); ++i)
			{
				const std::string t = Trim(a_lines[i]);
				if (!t.empty() && t.front() == '[' && t.back() == ']')
				{
					section = Lower(t.substr(1, t.size() - 2));
					if (section == wantSection) { lastInSection = i; }
					continue;
				}
				if (section != wantSection) { continue; }
				if (!t.empty()) { lastInSection = i; }
				const auto eq = t.find('=');
				if (eq != std::string::npos && t[0] != ';' && Lower(Trim(t.substr(0, eq))) == wantKey)
				{
					a_lines[i] = std::string(a_key) + "=" + a_value;
					return;
				}
			}
			const std::string newLine = std::string(a_key) + "=" + a_value;
			if (lastInSection != std::string::npos) { a_lines.insert(a_lines.begin() + static_cast<std::ptrdiff_t>(lastInSection + 1), newLine); }
			else
			{
				a_lines.push_back(std::string("[") + a_section + "]");
				a_lines.push_back(newLine);
			}
		}

		std::string F(float a_v) { return std::format("{:.2f}", a_v); }
	}

	void Init(const char* a_iniFileName)
	{
		iniPath = (std::filesystem::current_path() / "Data" / "SKSE" / "Plugins" / a_iniFileName).string();
		LoadFileValues();
	}

	bool Reload()
	{
		const bool ok = LoadFileValues();
		ApplyLogLevel();
		return ok;
	}

	bool Save()
	{
		std::vector<std::string> lines;
		{
			std::ifstream in(iniPath);
			std::string line;
			while (in && std::getline(in, line)) { lines.push_back(line); }
		}
		WriteKey(lines, "General", "bEnabled", enabled ? "1" : "0");
		WriteKey(lines, "General", "bShowFriendlyHits", showFriendlyHits ? "1" : "0");
		WriteKey(lines, "General", "fDuration", F(duration));
		WriteKey(lines, "General", "fSize", F(size));
		WriteKey(lines, "General", "fOpacity", F(opacity));
		WriteKey(lines, "General", "iColorR", std::to_string(colorR.load()));
		WriteKey(lines, "General", "iColorG", std::to_string(colorG.load()));
		WriteKey(lines, "General", "iColorB", std::to_string(colorB.load()));
		WriteKey(lines, "General", "iMaxMarkers", std::to_string(maxMarkers.load()));
		WriteKey(lines, "Ring", "fRingOffset", std::format("{:.0f}", ringOffset.load()));
		WriteKey(lines, "Ring", "fRadius", std::format("{:.0f}", radius.load()));
		WriteKey(lines, "Ring", "fCenterOffsetX", std::format("{:.0f}", centerOffsetX.load()));
		WriteKey(lines, "Ring", "fCenterOffsetY", std::format("{:.0f}", centerOffsetY.load()));
		WriteKey(lines, "Debug", "uLogLevel", std::to_string(logLevel.load()));

		std::ofstream out(iniPath, std::ios::trunc);
		if (!out)
		{
			logger::error("Save: could not open {} for writing", iniPath);
			return false;
		}
		for (const auto& line : lines) { out << line << '\n'; }
		logger::info("settings saved to {}", iniPath);
		return true;
	}

	void RestoreDefaults()
	{
		enabled = true;
		showFriendlyHits = true;
		duration = 1.5F;
		size = 1.0F;
		opacity = 1.0F;
		colorR = 255;
		colorG = 255;
		colorB = 255;
		maxMarkers = 8;
		ringOffset = -40.0F;
		radius = 305.0F;
		centerOffsetX = 0.0F;
		centerOffsetY = -48.0F;
		logger::info("settings: defaults restored in memory (not saved)");
	}

	void ApplyLogLevel()
	{
		const auto lvl = static_cast<spdlog::level::level_enum>(std::clamp<std::uint32_t>(logLevel.load(), 0u, 6u));
		SKSE::log::set_level(lvl, lvl);
	}

	const std::string& GetIniPath() { return iniPath; }
}
