#pragma once

namespace UI
{
	// Adds this mod's page to the menu framework (Apocrypha Menu Framework, or stock SKSE Menu Framework). Safe when
	// neither is present: the mod then runs from its INI. Call once at kDataLoaded.
	void Register();

	namespace SettingsPanel
	{
		void __stdcall Render();
	}
}
