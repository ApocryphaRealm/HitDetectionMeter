#pragma once

#include <string>

// Detection Meter (Maxsu, Nexus 63057): its art and its ring, read so our hit markers look like its detection markers
// and sit just outside them. Read at kDataLoaded and again on Reload.
namespace detectionmeter
{
	struct Geometry
	{
		bool        iniFound = false;      // MaxsuDetectionMeter.ini resolved through the Data folder
		bool        artFound = false;      // Meter_Frame.png AND Meter_Hostile.png resolved
		float       radiusX = 305.0F;      // [Display] RadiusX
		float       radiusY = 305.0F;      // [Display] RadiusY
		float       centerOffsetX = 0.0F;  // [Display] CenterOffsetX
		float       centerOffsetY = -48.0F;// [Display] CenterOffsetY
		float       flashScale = 0.25F;    // [AnimatedEffect] FlashScale - how much bigger a flash grows
		float       flashShift = 36.0F;    // [AnimatedEffect] FlashShift - how far out a flash moves
		std::string framePath;             // absolute paths, through MO2's VFS
		std::string fillPath;
	};

	void Read();
	Geometry Get();
	void Reread();   // Read() again and have the drawing pick the result up (Reload)
}

// The hits: a TESHitEvent sink keeps the recent hits on the player, one marker per aggressor.
namespace hits
{
	void Install();
	void Clear();                                          // on a game load
	void Inject(float a_bearingDeg, float a_strength);     // DevBench: a hit from this bearing, relative to the camera
	std::string StateJson();
}

// The drawing: an Apocrypha Menu Framework HUD element (render thread).
namespace hud
{
	void Register();
	void RefreshVisibility();   // main thread: whether a menu hides the HUD
	std::string StateJson();
}
