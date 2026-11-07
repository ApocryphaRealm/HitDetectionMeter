// Hit Detection Meter - own code, GPL-3.0-or-later. When something hits the player, a marker on a ring round the
// screen centre shows which way the hit came from. See the head of source/Indicator.cpp.
#include "PCH.h"

#include "DevBenchTool.h"
#include "Indicator.h"
#include "Settings.h"
#include "UI.h"

#include "utils/AddressLibraryGuard.h"
#include "utils/Logger.h"
#include "utils/Strings.h"

namespace
{
	void MessageHandler(SKSE::MessagingInterface::Message* a_msg)
	{
		if (!a_msg) { return; }
		switch (a_msg->type)
		{
		case SKSE::MessagingInterface::kPostLoad:
			DevBenchTool::Init(false);
			break;
		case SKSE::MessagingInterface::kDataLoaded:
			strings::Configure("HitDetectionMeter");
			detectionmeter::Read();
			hits::Install();
			hud::Register();
			UI::Register();
			DevBenchTool::Init(true);
			break;
		case SKSE::MessagingInterface::kPostLoadGame:
		case SKSE::MessagingInterface::kNewGame:
			hits::Clear();
			hud::RefreshVisibility();
			break;
		default:
			break;
		}
	}
}

SKSEPluginLoad(const SKSE::LoadInterface* a_skse)
{
	SKSE::log::init("HitDetectionMeter");
	// Address Library pre-check, BEFORE SKSE::Init, which opens the Address Library itself: a missing file gets a message
	// naming it and the plugin loads inert instead of CommonLibSSE-NG's bare failure line.
	if (!AddressLibraryGuard::Guard("Hit Detection Meter")) { return true; }
	SKSE::Init(a_skse);

	settings::Init("HitDetectionMeter.ini");
	settings::ApplyLogLevel();
	SKSE::log::describe_level("HitDetectionMeter.ini");
	logger::info("Hit Detection Meter {} loading", SKSE::PluginDeclaration::GetSingleton()->GetVersion().string("."));
	logger::info("Build line: {}", RUNTIME_LINE == 17 ? "Skyrim 1.7.x" : "SE 1.5.97 / AE 1.6.x");

	auto* messaging = SKSE::GetMessagingInterface();
	if (!messaging || !messaging->RegisterListener(MessageHandler))
	{
		logger::error("could not register the SKSE message listener; the mod stands down");
		return true;
	}
	return true;
}
