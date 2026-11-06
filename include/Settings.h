#pragma once

// Hit Direction Indicator - settings. Plain-file INI (never the Win32 profile API, which MO2's VFS does not redirect).
// Every compiled default here is the value the shipped INI carries (rule 16); RestoreDefaults() puts these back.

#include <atomic>
#include <cstdint>
#include <string>

namespace settings
{
	// [General]
	inline std::atomic<bool> enabled{ true };          // bEnabled - 0: no hit marker is drawn
	inline std::atomic<bool> showFriendlyHits{ true };  // bShowFriendlyHits - 0: hits from followers and non-hostile actors draw nothing
	inline std::atomic<float> duration{ 1.5F };         // fDuration - seconds a marker takes to fade out
	inline std::atomic<float> size{ 1.0F };             // fSize - a multiple of the art's own size
	inline std::atomic<float> opacity{ 1.0F };          // fOpacity - the strongest hit's opacity, 0 to 1
	inline std::atomic<int> colorR{ 255 };              // iColorR / iColorG / iColorB - the tint, 0 to 255 each; white draws the art as it is
	inline std::atomic<int> colorG{ 255 };
	inline std::atomic<int> colorB{ 255 };
	inline std::atomic<int> maxMarkers{ 8 };            // iMaxMarkers - markers on screen at once; the oldest goes first

	// [Ring] - the ring the markers sit on
	inline std::atomic<float> ringOffset{ 40.0F };      // fRingOffset - pixels outside Detection Meter's ring (or the ring below)
	inline std::atomic<float> radius{ 305.0F };         // fRadius - the ring's radius, used only when Detection Meter is not installed
	inline std::atomic<float> centerOffsetX{ 0.0F };    // fCenterOffsetX / fCenterOffsetY - the ring's centre from the screen centre,
	inline std::atomic<float> centerOffsetY{ -48.0F };  //   used only when Detection Meter is not installed (its own defaults)

	// [Debug]
	inline std::atomic<std::uint32_t> logLevel{ 2 };    // uLogLevel - 2 = info (rule 14)

	void Init(const char* a_iniFileName);
	bool Reload();
	bool Save();
	void RestoreDefaults();
	void ApplyLogLevel();
	const std::string& GetIniPath();
}
