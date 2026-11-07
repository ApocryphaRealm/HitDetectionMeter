// Hit Detection Meter - reading Detection Meter's own files, so the hit markers wear the detection markers' art
// (whichever art pack wins in the load order: the stock pictures, SkyALERT Redesign, SkyALERT Vanilla Presets...) and sit
// on a ring just outside theirs. The owner, 2026-11-06: "use whatever they have selected for detection meter", and
// "slightly offset because you could still get a damage indicator while sneaking".
//
// Learned from Detection Meter's source (MIT, github.com/max-su-2019/MaxsuDetectionMeter), not copied: it loads
// Data\SKSE\Plugins\MaxsuDetectionMeter\Meter_{Frame,NonHostile,Hostile}.png, draws each marker centred on a ring of
// RadiusX / RadiusY around the screen centre moved by CenterOffsetX / CenterOffsetY, turned to the bearing with the
// picture's TOP edge facing out, and scales the pictures (not the ring) by the screen size over 1920 x 1080.
#include "PCH.h"

#include "Indicator.h"

#include "utils/Logger.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <mutex>

namespace detectionmeter
{
	namespace
	{
		std::mutex g_lock;
		Geometry   g_geo;

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
	}

	void Read()
	{
		Geometry g;
		const auto plugins = std::filesystem::current_path() / "Data" / "SKSE" / "Plugins";
		const auto frame = plugins / "MaxsuDetectionMeter" / "Meter_Frame.png";
		const auto fill = plugins / "MaxsuDetectionMeter" / "Meter_Hostile.png";
		const auto ini = plugins / "MaxsuDetectionMeter.ini";
		std::error_code ec;
		g.artFound = std::filesystem::exists(frame, ec) && std::filesystem::exists(fill, ec);
		if (g.artFound)
		{
			g.framePath = frame.string();
			g.fillPath = fill.string();
		}

		std::ifstream in(ini);
		if (in)
		{
			g.iniFound = true;
			std::string line, section;
			if (std::getline(in, line))
			{
				if (line.size() >= 3 && static_cast<unsigned char>(line[0]) == 0xEF && static_cast<unsigned char>(line[1]) == 0xBB && static_cast<unsigned char>(line[2]) == 0xBF)
				{
					line.erase(0, 3);   // its INI is written with a UTF-8 byte-order mark
				}
				do
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
						const float v = std::stof(value);
						if (section == "display" && key == "radiusx") { g.radiusX = std::clamp(v, 0.0F, 2000.0F); }
						else if (section == "display" && key == "radiusy") { g.radiusY = std::clamp(v, 0.0F, 2000.0F); }
						else if (section == "display" && key == "centeroffsetx") { g.centerOffsetX = std::clamp(v, -1000.0F, 1000.0F); }
						else if (section == "display" && key == "centeroffsety") { g.centerOffsetY = std::clamp(v, -1000.0F, 1000.0F); }
						else if (section == "animatedeffect" && key == "flashscale") { g.flashScale = std::clamp(v, 0.0F, 5.0F); }
						else if (section == "animatedeffect" && key == "flashshift") { g.flashShift = std::clamp(v, 0.0F, 500.0F); }
					}
					catch (...)
					{
						logger::debug("Detection Meter INI: \"{}\" = \"{}\" is not a number; ignored", key, value);
					}
				} while (std::getline(in, line));
			}
		}

		if (g.artFound)
		{
			logger::info("Detection Meter art found: {} and {} - the hit markers wear it", g.framePath, g.fillPath);
		}
		else
		{
			logger::info("Detection Meter art not found under {} - the hit markers use the Nordic art", (plugins / "MaxsuDetectionMeter").string());
		}
		if (g.iniFound)
		{
			logger::info("Detection Meter INI {}: RadiusX={:.1f} RadiusY={:.1f} CenterOffsetX={:.1f} CenterOffsetY={:.1f} FlashScale={:.2f} FlashShift={:.1f}",
				ini.string(), g.radiusX, g.radiusY, g.centerOffsetX, g.centerOffsetY, g.flashScale, g.flashShift);
		}
		else
		{
			logger::info("Detection Meter INI not found at {} - the ring comes from this mod's own [Ring] settings", ini.string());
		}

		std::scoped_lock lock(g_lock);
		g_geo = g;
	}

	Geometry Get()
	{
		std::scoped_lock lock(g_lock);
		return g_geo;
	}
}
