#include "PCH.h"

#include "UI.h"

#include "SKSEMenuFramework.h"

#include "Indicator.h"
#include "PreciseSlider.h"
#include "Settings.h"

#include "utils/Logger.h"
#include "utils/Strings.h"
#include "utils/Toggle.h"

#include <atomic>
#include <functional>
#include <string>

namespace UI
{
	namespace
	{
		std::string statusMessage;

		void OnMainThread(std::function<void()> a_task)
		{
			if (auto* taskInterface = SKSE::GetTaskInterface()) { taskInterface->AddTask(std::move(a_task)); }
		}

		bool HasRequiredExports()
		{
			constexpr const char* required[] = {
				"AddSectionItem",
				"igTextV",
				"igTextDisabledV",
				"igTextWrappedV",
				"igSetTooltipV",
				"igSeparatorText",
				"igSliderInt",
				"igSliderFloat",
				// precise::SliderInt / SliderFloat (include/PreciseSlider.h, rule 68) ask these to tell a nudge from a drag.
				"igIsKeyDown_Nil",
				"igIsMouseDown_Nil",
				"igIsItemHovered",
				"igButton",
				"igSameLine",
				"igSpacing",
				"igPushItemWidth",
				"igPopItemWidth",
				// the on/off switch (utils/Toggle.h, rule 32)
				"igGetCursorScreenPos",
				"igGetWindowDrawList",
				"igGetFrameHeight",
				"igInvisibleButton",
				"igPushID_Str",
				"igPopID",
				"ImDrawList_AddRectFilled",
				"ImDrawList_AddCircleFilled"
			};
			for (const char* name : required)
			{
				if (!GetMenuFrameworkFunction<void*>(name))
				{
					logger::warn("The menu framework does not export \"{}\"", name);
					return false;
				}
			}
			return true;
		}

		void HelpMarker(const char* a_description)
		{
			ImGuiMCP::SameLine();
			ImGuiMCP::TextDisabled("%s", strings::TR("HDM_HelpMark", "(?)"));
			if (ImGuiMCP::IsItemHovered()) { ImGuiMCP::SetTooltip("%s", a_description); }
		}

		// A float setting through a precise slider (rule 68): one unit of the shown digit per nudge. Applies live.
		void FloatRow(const char* a_label, std::atomic<float>& a_value, float a_min, float a_max, const char* a_format, const char* a_help)
		{
			float v = a_value.load();
			if (precise::SliderFloat(a_label, &v, a_min, a_max, a_format)) { a_value = v; }
			HelpMarker(a_help);
		}

		void IntRow(const char* a_label, std::atomic<int>& a_value, int a_min, int a_max, const char* a_help)
		{
			int v = a_value.load();
			if (precise::SliderInt(a_label, &v, a_min, a_max, "%d")) { a_value = v; }
			HelpMarker(a_help);
		}

		void ToggleRow(const char* a_label, std::atomic<bool>& a_value, const char* a_help)
		{
			bool v = a_value.load();
			if (ImGuiMCP::Toggle(a_label, &v)) { a_value = v; }
			HelpMarker(a_help);
		}

		void RenderButtons()
		{
			ImGuiMCP::SeparatorText("");

			if (ImGuiMCP::Button(strings::TR("HDM_SaveBtn", "Save")))
			{
				statusMessage = strings::TR("HDM_StatusSaving", "Saving...");
				OnMainThread([]() {
					statusMessage = settings::Save() ? strings::TR("HDM_StatusSaved", "Settings saved.") : strings::TR("HDM_StatusSaveFail", "Could not write the INI. See the log for why.");
				});
			}
			HelpMarker(strings::TR("HDM_HelpSave", "Writes this page to the mod's INI so it survives a restart."));

			ImGuiMCP::SameLine();
			if (ImGuiMCP::Button(strings::TR("HDM_ReloadBtn", "Reload from INI")))
			{
				statusMessage = strings::TR("HDM_StatusReloading", "Reloading...");
				OnMainThread([]() {
					const bool ok = settings::Reload();
					detectionmeter::Reread();
					statusMessage = ok ? strings::TR("HDM_StatusReloaded", "Settings reloaded from the INI.") : strings::TR("HDM_StatusReloadFail", "Could not read the INI. See the log for why.");
				});
			}
			HelpMarker(strings::TR("HDM_HelpReload", "Throws away changes made here since the last save, reads the INI again, and reads Detection Meter's settings and art again."));

			ImGuiMCP::SameLine();
			if (ImGuiMCP::Button(strings::TR("HDM_RestoreBtn", "Restore defaults")))
			{
				settings::RestoreDefaults();
				statusMessage = strings::TR("HDM_StatusRestored", "Defaults restored. Press Save to keep them.");
			}
			HelpMarker(strings::TR("HDM_HelpRestore", "Puts this page back to its fresh-install values. Nothing is written until you press Save."));

			if (!statusMessage.empty()) { ImGuiMCP::TextWrapped("%s", statusMessage.c_str()); }
			ImGuiMCP::Spacing();
			ImGuiMCP::Text("%s", settings::GetIniPath().c_str());
		}
	}

	void Register()
	{
		if (!SKSEMenuFramework::IsInstalled())
		{
			logger::info("No menu framework is installed; settings are read from the INI only");
			return;
		}
		if (!HasRequiredExports())
		{
			logger::warn("The installed menu framework is older than this page needs. Settings are read from the INI only.");
			return;
		}
		SKSEMenuFramework::SetSection("Hit Detection Meter");
		SKSEMenuFramework::AddSectionItem("Settings", SettingsPanel::Render);
		logger::info("Registered the settings page with the menu framework");
	}

	void __stdcall SettingsPanel::Render()
	{
		strings::Tick();

		ImGuiMCP::TextWrapped("%s", strings::TR("HDM_Intro", "When something hits you, a marker on a ring round the middle of the screen shows which way the hit came from. Changes apply at once; press Save to keep them."));
		ImGuiMCP::Spacing();

		const auto geo = detectionmeter::Get();
		if (geo.artFound)
		{
			ImGuiMCP::TextWrapped("%s", strings::TR("HDM_StyleDM", "Detection Meter is installed: the markers wear its art, whichever pack you chose for it, on a ring just outside its own."));
		}
		else
		{
			ImGuiMCP::TextWrapped("%s", strings::TR("HDM_StyleNordic", "Detection Meter is not installed: the markers use this mod's own Nordic art."));
		}
		ImGuiMCP::Spacing();

		ImGuiMCP::PushItemWidth(260.0F);

		ImGuiMCP::SeparatorText(strings::TR("HDM_General", "Markers"));
		ToggleRow(strings::TR("HDM_Enable", "Show hit markers"), settings::enabled, strings::TR("HDM_HelpEnable", "Off: no marker is drawn."));
		ToggleRow(strings::TR("HDM_Friendly", "Show hits from friends"), settings::showFriendlyHits,
			strings::TR("HDM_HelpFriendly", "Off: hits from followers and from anyone not hostile to you draw no marker."));
		FloatRow(strings::TR("HDM_Duration", "Fade time"), settings::duration, 0.2F, 5.0F, "%.1f s", strings::TR("HDM_HelpDuration", "Seconds a marker takes to fade out after the hit."));
		FloatRow(strings::TR("HDM_Size", "Size"), settings::size, 0.25F, 3.0F, "%.2f", strings::TR("HDM_HelpSize", "A multiple of the art's own size. 1 matches Detection Meter's markers."));
		FloatRow(strings::TR("HDM_Opacity", "Opacity"), settings::opacity, 0.0F, 1.0F, "%.2f",
			strings::TR("HDM_HelpOpacity", "How solid the strongest hit's marker is. Power attacks draw at this; ordinary hits at seven tenths of it; blocked hits fainter."));
		IntRow(strings::TR("HDM_ColorR", "Tint red"), settings::colorR, 0, 255, strings::TR("HDM_HelpColor", "The marker's tint. White (255, 255, 255) draws the art as it is."));
		IntRow(strings::TR("HDM_ColorG", "Tint green"), settings::colorG, 0, 255, strings::TR("HDM_HelpColor", "The marker's tint. White (255, 255, 255) draws the art as it is."));
		IntRow(strings::TR("HDM_ColorB", "Tint blue"), settings::colorB, 0, 255, strings::TR("HDM_HelpColor", "The marker's tint. White (255, 255, 255) draws the art as it is."));
		IntRow(strings::TR("HDM_MaxMarkers", "Markers at once"), settings::maxMarkers, 1, 16, strings::TR("HDM_HelpMaxMarkers", "Each attacker has one marker; past this many, the oldest goes."));

		ImGuiMCP::SeparatorText(strings::TR("HDM_Ring", "Ring"));
		FloatRow(strings::TR("HDM_RingOffset", "Distance outside Detection Meter"), settings::ringOffset, -100.0F, 300.0F, "%.0f px",
			strings::TR("HDM_HelpRingOffset", "How far outside Detection Meter's ring the hit markers sit, so a hit taken while sneaking sits beside a detection marker, not on top of it."));
		if (geo.iniFound)
		{
			ImGuiMCP::TextDisabled("%s", strings::TR("HDM_RingFromDM", "The ring's size and centre come from Detection Meter's own settings."));
		}
		else
		{
			FloatRow(strings::TR("HDM_Radius", "Ring radius"), settings::radius, 0.0F, 1000.0F, "%.0f px", strings::TR("HDM_HelpRadius", "Used only without Detection Meter."));
			FloatRow(strings::TR("HDM_CenterX", "Ring centre, across"), settings::centerOffsetX, -500.0F, 500.0F, "%.0f px",
				strings::TR("HDM_HelpCenter", "Moves the ring's centre from the middle of the screen. Used only without Detection Meter."));
			FloatRow(strings::TR("HDM_CenterY", "Ring centre, down"), settings::centerOffsetY, -500.0F, 500.0F, "%.0f px",
				strings::TR("HDM_HelpCenter", "Moves the ring's centre from the middle of the screen. Used only without Detection Meter."));
		}

		ImGuiMCP::PopItemWidth();

		RenderButtons();
	}
}
