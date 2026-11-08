// Hit Detection Meter - own code, GPL-3.0-or-later (NOTICE.md).
//
// Whenever an enemy hits the player, a marker at the edge of a ring round the screen centre shows which way the hit
// came from, and fades. The idea and the numbers are ported from our own Oblivion Remastered HUD Position Manager's
// damage direction indicator (src/Hud.cpp: a flash per hit with its angle, its time and a strength, faded linearly
// over 1500 ms, opacity = strength x (1 - age), the oldest dropped past the cap). What changes for Skyrim:
//   * a hit is the game's own TESHitEvent on the player, not a drop in health, so it knows WHO hit - the aggressor,
//     or the shooter of a projectile - and draws one marker for that actor, not one per hostile nearby;
//   * the marker keeps the aggressor's position and its bearing is measured every frame against the CAMERA yaw
//     (PlayerCamera RUNTIME_DATA2 yaw, the same yaw Dragon's Eye Minimap's compass ring reads), so a marker keeps
//     pointing at the hit's source while the view turns;
//   * it ages only while it is drawn, so a pause or a menu does not eat the fade;
//   * it draws through the Apocrypha Menu Framework's HUD-element API (render thread) with Detection Meter's own
//     art when that mod is installed, on a ring fRingOffset pixels outside Detection Meter's ring, else our Nordic art.
//
// Threads: the hit and menu sinks run on the main thread; the draw callback runs on the render thread and touches no
// game lock - no RE::UI menu lookups, no BSFixedString, no form lookups (past-mistakes library, "A HUD widget callback
// runs on the RENDER thread"). It reads plain values only: the player's position, the camera yaw and
// UI::GameIsPaused() (an integer).
#include "PCH.h"

#include "Indicator.h"

#include "SKSEMenuFramework.h"
#include "Settings.h"

#include "utils/Logger.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <format>
#include <mutex>
#include <numbers>
#include <string>
#include <vector>

namespace
{
	constexpr float kPi = std::numbers::pi_v<float>;

	// ---- the hits ---------------------------------------------------------------------------------------------------

	struct Flash
	{
		RE::FormID   aggressor = 0;        // 0 for an injected (DevBench) hit
		RE::NiPoint3 from{};               // where the aggressor stood when it hit
		bool         fixedAngle = false;   // an injected hit: angle is relative to the camera already
		float        angle = 0.0F;         // degrees, used when fixedAngle
		float        strength = 1.0F;      // 0..1: opacity at the start of the fade
		float        age = 0.0F;           // seconds drawn so far
	};

	std::mutex         g_flashLock;
	std::vector<Flash> g_flashes;

	// counters for the DevBench state tool (rule 31)
	std::atomic<std::uint32_t> g_hitsSeen{ 0 }, g_hitsShown{ 0 }, g_ignoredSelf{ 0 }, g_ignoredFriendly{ 0 }, g_ignoredNoActor{ 0 }, g_ignoredOff{ 0 };
	std::mutex  g_lastLock;
	std::string g_lastHit = "none";

	// a name for a JSON string: no quote, backslash or control character can break the document
	std::string JsonSafe(const char* a_s)
	{
		std::string out;
		for (const char* p = a_s ? a_s : ""; *p; ++p)
		{
			if (*p != '"' && *p != 0x5C && static_cast<unsigned char>(*p) >= 0x20) { out += *p; }
		}
		return out;
	}

	float StrengthFor(const RE::TESHitEvent& a_event)
	{
		using F = RE::TESHitEvent::Flag;
		// The event carries no damage, so the size of a hit is read from what kind of hit it was: a power attack draws
		// full, a bash nearly so, an ordinary hit at 0.7, and a hit the player blocked at six tenths of that.
		float s = 0.7F;
		if (a_event.flags.any(F::kPowerAttack)) { s = 1.0F; }
		else if (a_event.flags.any(F::kBashAttack)) { s = 0.85F; }
		if (a_event.flags.any(F::kHitBlocked)) { s *= 0.6F; }
		return std::clamp(s, 0.3F, 1.0F);
	}

	void AddFlash(const Flash& a_new, const char* a_who)
	{
		std::size_t n = 0;
		bool refreshed = false;
		{
			std::scoped_lock lock(g_flashLock);
			const float dur = std::max(settings::duration.load(), 0.2F);
			if (a_new.aggressor != 0)
			{
				// one marker per aggressor: a weapon and its enchantment, or a spell held on the player, send a hit event
				// each, and a flurry from one enemy should brighten its marker, not stack copies at one bearing
				for (auto& f : g_flashes)
				{
					if (f.aggressor == a_new.aggressor)
					{
						const float left = f.strength * (1.0F - std::clamp(f.age / dur, 0.0F, 1.0F));
						f.strength = std::max(a_new.strength, left);
						f.from = a_new.from;
						f.age = 0.0F;
						refreshed = true;
						break;
					}
				}
			}
			if (!refreshed) { g_flashes.push_back(a_new); }
			const auto cap = static_cast<std::size_t>(std::clamp(settings::maxMarkers.load(), 1, 32));
			while (g_flashes.size() > cap)
			{
				const auto oldest = std::max_element(g_flashes.begin(), g_flashes.end(), [](const Flash& a, const Flash& b) { return a.age < b.age; });
				g_flashes.erase(oldest);
			}
			n = g_flashes.size();
		}
		++g_hitsShown;
		logger::debug("hit marker: {} {} (strength {:.2f}); {} marker(s) up", refreshed ? "refreshed for" : "added for", a_who, a_new.strength, n);
	}

	// Every place the camera's yaw can be read, in degrees, so a run shows which one follows the rendered view (NaN = not
	// available now). 0 = north (+Y), clockwise, as GetAngleZ and the hit bearing.
	struct YawSources
	{
		float niCamera = NAN;      // Main::WorldRootCamera() world rotation, column 0 (a Gamebryo camera looks down local +X) - the one used
		float w2c[3] = { NAN, NAN, NAN };   // that camera's worldToCam matrix (what HUD projection uses), rows 0..2 read as directions
		float root0 = NAN, root1 = NAN;     // PlayerCamera::cameraRoot world rotation, columns 0 and 1
		float thirdPerson = NAN;   // player heading + ThirdPersonState freeRotation.x (only while free rotation is on)
		float tpsCurrentYaw = NAN, tpsTargetYaw = NAN, tpsFreeX = NAN;   // ThirdPersonState's own fields, raw
		float runtimeData2 = NAN;  // PlayerCamera RUNTIME_DATA2 yaw
		float body = NAN;          // the player's heading
	};
	YawSources ReadYaws(RE::PlayerCharacter* a_player);
	float CameraYawDeg(RE::PlayerCharacter* a_player);
	std::string YawText(const YawSources& a_y);

	class HitSink final : public RE::BSTEventSink<RE::TESHitEvent>
	{
	public:
		static HitSink* Get()
		{
			static HitSink sink;
			return &sink;
		}

		RE::BSEventNotifyControl ProcessEvent(const RE::TESHitEvent* a_event, RE::BSTEventSource<RE::TESHitEvent>*) override
		{
			if (!a_event) { return RE::BSEventNotifyControl::kContinue; }
			auto* player = RE::PlayerCharacter::GetSingleton();
			if (!player || a_event->target.get() != player) { return RE::BSEventNotifyControl::kContinue; }
			++g_hitsSeen;
			if (!settings::enabled)
			{
				++g_ignoredOff;
				return RE::BSEventNotifyControl::kContinue;
			}

			// the aggressor: the cause itself when it is an actor, else the shooter of the projectile that is the cause
			RE::TESObjectREFR* cause = a_event->cause.get();
			RE::Actor* aggressor = cause ? cause->As<RE::Actor>() : nullptr;
			if (!aggressor && cause)
			{
				if (auto* proj = cause->As<RE::Projectile>())
				{
					auto shooter = proj->GetProjectileRuntimeData().shooter.get();
					aggressor = shooter ? shooter->As<RE::Actor>() : nullptr;
				}
			}
			if (!aggressor)
			{
				++g_ignoredNoActor;
				logger::debug("hit on the player ignored: the cause ({:08X}) is not an actor or an actor's projectile", cause ? cause->GetFormID() : 0u);
				return RE::BSEventNotifyControl::kContinue;
			}
			if (aggressor == player)
			{
				++g_ignoredSelf;
				logger::debug("hit on the player ignored: the player hit themself (source {:08X})", a_event->source);
				return RE::BSEventNotifyControl::kContinue;
			}
			const bool teammate = aggressor->IsPlayerTeammate();
			const bool hostile = aggressor->IsHostileToActor(player);
			if (!settings::showFriendlyHits && (teammate || !hostile))
			{
				++g_ignoredFriendly;
				logger::debug("hit on the player ignored: {:08X} is friendly (teammate {}, hostile {}) and bShowFriendlyHits is 0", aggressor->GetFormID(), teammate, hostile);
				return RE::BSEventNotifyControl::kContinue;
			}

			Flash f;
			f.aggressor = aggressor->GetFormID();
			f.from = aggressor->GetPosition();
			f.strength = StrengthFor(*a_event);
			const RE::NiPoint3 me = player->GetPosition();
			const float bearing = std::atan2(f.from.x - me.x, f.from.y - me.y) * 180.0F / kPi;
			const auto name = aggressor->GetDisplayFullName();
			const std::string who = std::format("{:08X} \"{}\"", f.aggressor, name ? name : "");
			{
				std::scoped_lock lock(g_lastLock);
				g_lastHit = std::format(R"({{"aggressor":"{:08X}","name":"{}","worldBearing":{:.1f},"distance":{:.0f},"flags":{},"strength":{:.2f},"source":"{:08X}","projectile":"{:08X}"}})",
					f.aggressor, JsonSafe(name), bearing, me.GetDistance(f.from), static_cast<int>(a_event->flags.underlying()), f.strength, a_event->source, a_event->projectile);
			}
			logger::debug("hit on the player from {} at world bearing {:.0f}, {:.0f} units away, flags {:#x}, cause {:08X}", who, bearing, me.GetDistance(f.from),
				static_cast<int>(a_event->flags.underlying()), cause ? cause->GetFormID() : 0u);
			{
				// every yaw source, so a run shows which one moves when the camera turns (NaN = not available now)
				const auto y = ReadYaws(player);
				logger::debug("camera yaw sources at hit: {}", YawText(y));
			}
			// the numbers behind a marker in the shipped (info) log too, at most once a second, as the Oblivion indicator does
			static auto s_lastInfo = std::chrono::steady_clock::time_point{};
			const auto now = std::chrono::steady_clock::now();
			if (now - s_lastInfo >= std::chrono::seconds(1))
			{
				s_lastInfo = now;
				const float yawDeg = CameraYawDeg(player);
				float rel = std::fmod(bearing - yawDeg, 360.0F);
				if (rel > 180.0F) { rel -= 360.0F; }
				if (rel <= -180.0F) { rel += 360.0F; }
				logger::info("hit from {}: world bearing {:.0f}, camera yaw {:.0f} -> marker at {:.0f} (0 top, 90 right, -90 left), strength {:.2f}", who, bearing, yawDeg, rel, f.strength);
			}
			AddFlash(f, who.c_str());
			return RE::BSEventNotifyControl::kContinue;
		}
	};

	// ---- what hides the HUD (main thread) ---------------------------------------------------------------------------

	std::atomic<bool> g_menuHidden{ false };
	std::mutex        g_hiddenWhyLock;
	std::string       g_hiddenWhy = "not checked yet";

	class MenuSink final : public RE::BSTEventSink<RE::MenuOpenCloseEvent>
	{
	public:
		static MenuSink* Get()
		{
			static MenuSink sink;
			return &sink;
		}

		RE::BSEventNotifyControl ProcessEvent(const RE::MenuOpenCloseEvent* a_event, RE::BSTEventSource<RE::MenuOpenCloseEvent>*) override
		{
			if (!a_event) { return RE::BSEventNotifyControl::kContinue; }
			// read the menu stack once it has settled, a task later on the main thread
			if (auto* tasks = SKSE::GetTaskInterface()) { tasks->AddTask([]() { hud::RefreshVisibility(); }); }
			return RE::BSEventNotifyControl::kContinue;
		}
	};

	// ---- the drawing (render thread) --------------------------------------------------------------------------------

	std::atomic<std::uint32_t> g_geoGeneration{ 0 };   // stepped by every Detection Meter read, so the draw picks the new art up

	using GetIOFn = ImGuiMCP::ImGuiIO* (*)();
	using GetDrawListFn = ImGuiMCP::ImDrawList* (*)();
	using AddImageQuadFn = void (*)(ImGuiMCP::ImDrawList*, ImGuiMCP::ImTextureID, const ImGuiMCP::ImVec2, const ImGuiMCP::ImVec2, const ImGuiMCP::ImVec2, const ImGuiMCP::ImVec2,
		const ImGuiMCP::ImVec2, const ImGuiMCP::ImVec2, const ImGuiMCP::ImVec2, const ImGuiMCP::ImVec2, ImGuiMCP::ImU32);
	using AddPolylineFn = void (*)(ImGuiMCP::ImDrawList*, const ImGuiMCP::ImVec2*, int, ImGuiMCP::ImU32, ImGuiMCP::ImDrawFlags, float);
	using LoadTextureFn = ImGuiMCP::ImTextureID (*)(const char*, ImGuiMCP::ImVec2*);

	struct Api
	{
		GetIOFn        getIO = nullptr;
		GetDrawListFn  background = nullptr;
		AddImageQuadFn addImageQuad = nullptr;
		AddPolylineFn  addPolyline = nullptr;
		LoadTextureFn  loadTexture = nullptr;
	} g_api;

	struct Texture
	{
		ImGuiMCP::ImTextureID id = nullptr;
		float                 w = 0.0F, h = 0.0F;   // its pixels
		bool                  tried = false;
	};

	// Art: 0 none yet, 1 Detection Meter's (frame + hostile fill), 2 the Nordic marker, 3 a plain drawn arc (no texture)
	enum Art : int { kArtNone = 0, kArtDetectionMeter = 1, kArtNordic = 2, kArtLines = 3 };
	std::atomic<int>           g_art{ kArtNone };
	std::atomic<std::uint64_t> g_framesDrawn{ 0 };
	std::atomic<int>           g_markersLastFrame{ 0 };
	std::atomic<bool>          g_registered{ false };
	std::atomic<float>         g_markerW{ 0.0F }, g_markerH{ 0.0F };   // the marker's drawn size before the pop, in screen pixels

	constexpr float kNordicW = 160.0F, kNordicH = 44.0F;   // the Nordic art's size on a 1080p screen (its PNG is drawn at 2x)

	Texture LoadTex(const std::string& a_path)
	{
		Texture t;
		t.tried = true;
		if (!g_api.loadTexture || a_path.empty()) { return t; }
		ImGuiMCP::ImVec2 size{ 0.0F, 0.0F };
		t.id = g_api.loadTexture(a_path.c_str(), &size);
		t.w = size.x;
		t.h = size.y;
		if (t.id && (t.w <= 0.0F || t.h <= 0.0F))
		{
			logger::warn("texture {} loaded with no size reported; it is not drawn", a_path);
			t.id = nullptr;
		}
		logger::info("texture {}: {} ({:.0f} x {:.0f})", a_path, t.id ? "loaded" : "NOT loaded", t.w, t.h);
		return t;
	}

	ImGuiMCP::ImVec2 Rotate(float a_x, float a_y, float a_cos, float a_sin) { return { a_x * a_cos - a_y * a_sin, a_x * a_sin + a_y * a_cos }; }

	// One picture centred at (a_cx, a_cy), a_w x a_h on screen, turned a_deg clockwise (0: its top edge faces up).
	void DrawRotated(ImGuiMCP::ImDrawList* a_dl, ImGuiMCP::ImTextureID a_tex, float a_cx, float a_cy, float a_w, float a_h, float a_deg, ImGuiMCP::ImU32 a_col)
	{
		const float r = a_deg * kPi / 180.0F, c = std::cos(r), s = std::sin(r);
		const auto p0 = Rotate(-a_w * 0.5F, -a_h * 0.5F, c, s), p1 = Rotate(a_w * 0.5F, -a_h * 0.5F, c, s);
		const auto p2 = Rotate(a_w * 0.5F, a_h * 0.5F, c, s), p3 = Rotate(-a_w * 0.5F, a_h * 0.5F, c, s);
		g_api.addImageQuad(a_dl, a_tex, { a_cx + p0.x, a_cy + p0.y }, { a_cx + p1.x, a_cy + p1.y }, { a_cx + p2.x, a_cy + p2.y }, { a_cx + p3.x, a_cy + p3.y },
			{ 0.0F, 0.0F }, { 1.0F, 0.0F }, { 1.0F, 1.0F }, { 0.0F, 1.0F }, a_col);
	}

	float Wrap180(float a_deg)
	{
		float r = std::fmod(a_deg, 360.0F);
		if (r > 180.0F) { r -= 360.0F; }
		if (r <= -180.0F) { r += 360.0F; }
		return r;
	}

	// The ways the camera's yaw can be read. 2026-11-07 18:55 run (True Directional Movement + SmoothCam): NiCamera,
	// RUNTIME_DATA2 and heading+freeRotation all agreed (186.1) and none moved across a reported 60-90 degree camera turn,
	// so every further candidate is read too, and Draw logs whenever any of them moves (TraceYaws) - the run shows which
	// one follows the view. Compass Navigation Overhaul and Dragon's Eye Minimap read RUNTIME_DATA2 yaw for their compasses.
	YawSources ReadYaws(RE::PlayerCharacter* a_player)
	{
		YawSources y;
		constexpr float k = 180.0F / kPi;
		auto dirYaw = [](float a_x, float a_y) { return a_x * a_x + a_y * a_y > 1e-6F ? std::atan2(a_x, a_y) * k : NAN; };
		if (auto* cam = RE::Main::WorldRootCamera())
		{
			const auto& m = cam->world.rotate;
			y.niCamera = dirYaw(m.entry[0][0], m.entry[1][0]);
			const auto& w = cam->GetRuntimeData().worldToCam;
			for (int r = 0; r < 3; ++r) { y.w2c[r] = dirYaw(w[r][0], w[r][1]); }
		}
		if (a_player) { y.body = a_player->GetAngleZ() * k; }
		if (auto* camera = RE::PlayerCamera::GetSingleton())
		{
			y.runtimeData2 = camera->GetRuntimeData2().yaw * k;
			if (auto* root = camera->cameraRoot.get())
			{
				const auto& m = root->world.rotate;
				y.root0 = dirYaw(m.entry[0][0], m.entry[1][0]);
				y.root1 = dirYaw(m.entry[0][1], m.entry[1][1]);
			}
			if (camera->IsInThirdPerson() && a_player)
			{
				if (auto* tps = static_cast<RE::ThirdPersonState*>(camera->currentState.get()))
				{
					y.thirdPerson = (a_player->GetAngleZ() + (tps->freeRotationEnabled ? tps->freeRotation.x : 0.0F)) * k;
					y.tpsCurrentYaw = tps->currentYaw * k;
					y.tpsTargetYaw = tps->targetYaw * k;
					y.tpsFreeX = tps->freeRotation.x * k;
				}
			}
		}
		return y;
	}

	std::string YawText(const YawSources& a_y)
	{
		return std::format("NiCamera {:.1f} (used) | worldToCam rows {:.1f} {:.1f} {:.1f} | cameraRoot cols {:.1f} {:.1f} | heading+freeRotation {:.1f} | "
			"TPS currentYaw {:.1f} targetYaw {:.1f} freeRotation.x {:.1f} | RUNTIME_DATA2 {:.1f} | body {:.1f}",
			a_y.niCamera, a_y.w2c[0], a_y.w2c[1], a_y.w2c[2], a_y.root0, a_y.root1, a_y.thirdPerson, a_y.tpsCurrentYaw, a_y.tpsTargetYaw, a_y.tpsFreeX,
			a_y.runtimeData2, a_y.body);
	}

	// A debug trace of the yaw sources over time (render thread, every frame): one line whenever any source has moved more
	// than 3 degrees since the last line, at most four a second - so a camera turn shows up whether or not a hit lands.
	void TraceYaws(RE::PlayerCharacter* a_player)
	{
		if (!spdlog::default_logger()->should_log(spdlog::level::debug)) { return; }
		static YawSources s_last;
		static auto s_at = std::chrono::steady_clock::time_point{};
		const auto now = std::chrono::steady_clock::now();
		if (now - s_at < std::chrono::milliseconds(250)) { return; }
		const auto y = ReadYaws(a_player);
		const float a[] = { y.niCamera, y.w2c[0], y.w2c[1], y.w2c[2], y.root0, y.root1, y.thirdPerson, y.tpsCurrentYaw, y.tpsTargetYaw, y.tpsFreeX, y.runtimeData2, y.body };
		const float b[] = { s_last.niCamera, s_last.w2c[0], s_last.w2c[1], s_last.w2c[2], s_last.root0, s_last.root1, s_last.thirdPerson, s_last.tpsCurrentYaw,
			s_last.tpsTargetYaw, s_last.tpsFreeX, s_last.runtimeData2, s_last.body };
		bool moved = false;
		for (std::size_t i = 0; i < std::size(a); ++i)
		{
			if (std::isnan(a[i]) != std::isnan(b[i]) || (!std::isnan(a[i]) && std::abs(Wrap180(a[i] - b[i])) > 3.0F)) { moved = true; break; }
		}
		if (!moved) { return; }
		s_last = y;
		s_at = now;
		logger::debug("camera yaw sources moved: {}", YawText(y));
	}

	// The camera's yaw in degrees (Dragon's Eye Minimap's PlayerHeading: the way the camera looks, not the body).
	float CameraYawDeg(RE::PlayerCharacter* a_player)
	{
		const auto y = ReadYaws(a_player);
		if (!std::isnan(y.niCamera)) { return y.niCamera; }
		if (!std::isnan(y.thirdPerson)) { return y.thirdPerson; }
		return std::isnan(y.body) ? 0.0F : y.body;
	}

	void __stdcall Draw()
	{
		static bool s_wasHidden = true;
		auto setHidden = [](bool a_hidden, const char* a_why) {
			if (a_hidden != s_wasHidden)
			{
				s_wasHidden = a_hidden;
				logger::debug("hit markers {}{}", a_hidden ? "hidden: " : "shown", a_hidden ? a_why : "");
			}
		};

		if (!settings::enabled) { setHidden(true, "disabled"); return; }
		auto* ui = RE::UI::GetSingleton();
		if (!ui || ui->GameIsPaused() || g_menuHidden.load(std::memory_order_acquire)) { setHidden(true, "paused or a menu is up"); return; }
		if (!g_api.getIO || !g_api.background || !g_api.addImageQuad) { return; }
		auto* io = g_api.getIO();
		if (!io || io->DisplaySize.x <= 0.0F || io->DisplaySize.y <= 0.0F) { return; }   // minimised: no size to place on
		TraceYaws(RE::PlayerCharacter::GetSingleton());

		const float dur = std::max(settings::duration.load(), 0.2F);
		const float dt = std::clamp(io->DeltaTime, 0.0F, 0.1F);
		std::vector<Flash> live;
		{
			std::scoped_lock lock(g_flashLock);
			if (g_flashes.empty()) { g_markersLastFrame = 0; setHidden(false, ""); return; }
			for (auto& f : g_flashes) { f.age += dt; }
			std::erase_if(g_flashes, [dur](const Flash& f) { return f.age >= dur; });
			live = g_flashes;
		}
		setHidden(false, "");
		if (live.empty()) { g_markersLastFrame = 0; return; }

		auto* player = RE::PlayerCharacter::GetSingleton();
		if (!player) { return; }
		const RE::NiPoint3 me = player->GetPosition();
		const float yaw = CameraYawDeg(player);

		// the art, chosen once: Detection Meter's when it was found, else the Nordic marker, else plain lines
		static Texture s_frame, s_fill, s_nordic;
		static detectionmeter::Geometry s_geo;
		static bool s_geoRead = false;
		static std::uint32_t s_geoGen = 0;
		if (!s_geoRead || s_geoGen != g_geoGeneration.load())
		{
			s_geo = detectionmeter::Get();   // only when Reload read it again
			s_geoGen = g_geoGeneration.load();
			s_geoRead = true;
			s_frame = {};
			s_fill = {};
			g_art = kArtNone;
		}
		if (g_art.load() == kArtNone)
		{
			if (s_geo.artFound)
			{
				if (!s_frame.tried) { s_frame = LoadTex(s_geo.framePath); }
				if (!s_fill.tried) { s_fill = LoadTex(s_geo.fillPath); }
			}
			if (s_frame.id && s_fill.id) { g_art = kArtDetectionMeter; }
			else
			{
				if (!s_nordic.tried)
				{
					s_nordic = LoadTex((std::filesystem::current_path() / "Data" / "SKSE" / "Plugins" / "HitDetectionMeter" / "HitMarker_Nordic.png").string());
				}
				g_art = s_nordic.id ? kArtNordic : kArtLines;
				if (!s_nordic.id) { logger::warn("no marker art could be loaded; the hit markers are drawn as plain arcs"); }
			}
			logger::info("hit markers drawn with {}", g_art == kArtDetectionMeter ? "Detection Meter's art" : g_art == kArtNordic ? "the Nordic art" : "plain arcs");
		}

		// the ring: Detection Meter's own when its INI was read, else ours; ours sits fRingOffset outside it
		const bool dmRing = s_geo.iniFound;
		const float offset = settings::ringOffset.load();
		const float rx = (dmRing ? s_geo.radiusX : settings::radius.load()) + offset;
		const float ry = (dmRing ? s_geo.radiusY : settings::radius.load()) + offset;
		const float cx = io->DisplaySize.x * 0.5F + (dmRing ? s_geo.centerOffsetX : settings::centerOffsetX.load());
		const float cy = io->DisplaySize.y * 0.5F + (dmRing ? s_geo.centerOffsetY : settings::centerOffsetY.load());
		const float flashScale = s_geo.flashScale, flashShift = s_geo.flashShift;
		// pictures scale with the screen, as Detection Meter's do (its art is drawn for 1920 x 1080); the ring does not
		const float resX = io->DisplaySize.x / 1920.0F, resY = io->DisplaySize.y / 1080.0F;
		const float sx = resX * settings::size.load();
		const float sy = resY * settings::size.load();
		// Detection Meter's own marker size, worked out the way its Renderer does it (read from its source, 2026-11-07):
		// the PNG's pixels times screen width / 1920 and height / 1080, kept as whole pixels (an int), and no INI key scales
		// it. fSize multiplies that, so 1.0 is exactly Detection Meter's size.
		auto dmPx = [&](const Texture& a_t) {
			return ImGuiMCP::ImVec2{ std::floor(a_t.w * resX) * settings::size.load(), std::floor(a_t.h * resY) * settings::size.load() };
		};
		const float opacity = std::clamp(settings::opacity.load(), 0.0F, 1.0F);
		const int r = settings::colorR.load(), g = settings::colorG.load(), b = settings::colorB.load();

		auto* dl = g_api.background();
		if (!dl) { return; }
		const int art = g_art.load();
		const ImGuiMCP::ImVec2 framePx = dmPx(s_frame), fillPx = dmPx(s_fill);
		const ImGuiMCP::ImVec2 markerPx = art == kArtDetectionMeter ? framePx : art == kArtNordic ? ImGuiMCP::ImVec2{ kNordicW * sx, kNordicH * sy } : ImGuiMCP::ImVec2{ 0.0F, 0.0F };
		if (markerPx.x != g_markerW.load() || markerPx.y != g_markerH.load())
		{
			g_markerW = markerPx.x;
			g_markerH = markerPx.y;
			logger::info("hit marker size {:.0f} x {:.0f} px on a {:.0f} x {:.0f} screen ({}; fSize {:.2f})", markerPx.x, markerPx.y, io->DisplaySize.x, io->DisplaySize.y,
				art == kArtDetectionMeter ? "Detection Meter's frame PNG at its own size" : art == kArtNordic ? "the Nordic art" : "plain arcs", settings::size.load());
		}
		int drawn = 0;
		for (const auto& f : live)
		{
			const float t = std::clamp(f.age / dur, 0.0F, 1.0F);
			const float alpha = std::clamp(f.strength * (1.0F - t) * opacity, 0.0F, 1.0F);
			if (alpha <= 0.004F) { continue; }
			const float rel = f.fixedAngle ? f.angle : Wrap180(std::atan2(f.from.x - me.x, f.from.y - me.y) * 180.0F / kPi - yaw);
			// a short pop when the hit lands, like Detection Meter's flash: bigger and a little further out, settling in 0.2 s
			const float pop = std::clamp(1.0F - f.age / 0.2F, 0.0F, 1.0F);
			const float grow = 1.0F + flashScale * pop;
			const float rad = rel * kPi / 180.0F;
			const float px = cx + (rx + flashShift * (grow - 1.0F)) * std::sin(rad);
			const float py = cy - (ry + flashShift * (grow - 1.0F)) * std::cos(rad);
			const auto col = IM_COL32(r, g, b, static_cast<int>(alpha * 255.0F));
			if (art == kArtDetectionMeter)
			{
				DrawRotated(dl, s_frame.id, px, py, framePx.x * grow, framePx.y * grow, rel, col);
				DrawRotated(dl, s_fill.id, px, py, fillPx.x * grow, fillPx.y * grow, rel, col);
			}
			else if (art == kArtNordic)
			{
				DrawRotated(dl, s_nordic.id, px, py, kNordicW * sx * grow, kNordicH * sy * grow, rel, col);
			}
			else if (g_api.addPolyline)
			{
				// the last resort: an arc of the ring, blood red, so a hit still shows without any picture
				ImGuiMCP::ImVec2 pts[13];
				for (int k = 0; k < 13; ++k)
				{
					const float a = (rel - 14.0F + 28.0F * static_cast<float>(k) / 12.0F) * kPi / 180.0F;
					pts[k] = { cx + rx * std::sin(a), cy - ry * std::cos(a) };
				}
				g_api.addPolyline(dl, pts, 13, IM_COL32(150, 30, 20, static_cast<int>(alpha * 255.0F)), 0, 6.0F * sy);
			}
			++drawn;
		}
		g_markersLastFrame = drawn;
		++g_framesDrawn;
	}
}

namespace hits
{
	void Install()
	{
		auto* holder = RE::ScriptEventSourceHolder::GetSingleton();
		if (!holder)
		{
			logger::error("no script event source holder; hits cannot be seen and no marker will be drawn");
			return;
		}
		holder->AddEventSink<RE::TESHitEvent>(HitSink::Get());
		logger::info("listening for hits on the player (TESHitEvent)");
		if (auto* ui = RE::UI::GetSingleton())
		{
			ui->AddEventSink<RE::MenuOpenCloseEvent>(MenuSink::Get());
			logger::info("listening for menus (MenuOpenCloseEvent)");
		}
		else
		{
			logger::warn("no UI singleton at kDataLoaded; menus will not hide the markers");
		}
	}

	void Clear()
	{
		std::scoped_lock lock(g_flashLock);
		if (!g_flashes.empty()) { logger::debug("game loaded: {} marker(s) cleared", g_flashes.size()); }
		g_flashes.clear();
	}

	void Inject(float a_bearingDeg, float a_strength)
	{
		Flash f;
		f.fixedAngle = true;
		f.angle = Wrap180(a_bearingDeg);
		f.strength = std::clamp(a_strength, 0.0F, 1.0F);
		AddFlash(f, std::format("an injected hit at {:.0f} degrees", f.angle).c_str());
	}

	std::string StateJson()
	{
		std::string flashes = "[";
		{
			std::scoped_lock lock(g_flashLock);
			for (std::size_t i = 0; i < g_flashes.size(); ++i)
			{
				const auto& f = g_flashes[i];
				flashes += std::format(R"({}{{"aggressor":"{:08X}","injected":{},"angle":{:.1f},"strength":{:.2f},"age":{:.2f}}})", i ? "," : "", f.aggressor,
					f.fixedAngle ? "true" : "false", f.angle, f.strength, f.age);
			}
		}
		flashes += "]";
		std::string last;
		{
			std::scoped_lock lock(g_lastLock);
			last = g_lastHit;
		}
		return std::format(R"({{"hitsSeen":{},"hitsShown":{},"ignoredSelf":{},"ignoredFriendly":{},"ignoredNoActor":{},"ignoredDisabled":{},"markers":{},"lastHit":{}}})",
			g_hitsSeen.load(), g_hitsShown.load(), g_ignoredSelf.load(), g_ignoredFriendly.load(), g_ignoredNoActor.load(), g_ignoredOff.load(), flashes,
			last == "none" ? std::string("null") : last);
	}
}

namespace hud
{
	void Register()
	{
		if (!SKSEMenuFramework::IsInstalled())
		{
			logger::error("the Apocrypha Menu Framework (or SKSE Menu Framework) is not installed; hit markers cannot be drawn");
			return;
		}
		g_api.getIO = GetMenuFrameworkFunction<GetIOFn>("igGetIO");
		g_api.background = GetMenuFrameworkFunction<GetDrawListFn>("igGetBackgroundDrawList_Nil");
		g_api.addImageQuad = GetMenuFrameworkFunction<AddImageQuadFn>("ImDrawList_AddImageQuad");
		g_api.addPolyline = GetMenuFrameworkFunction<AddPolylineFn>("ImDrawList_AddPolyline");
		g_api.loadTexture = GetMenuFrameworkFunction<LoadTextureFn>("LoadTexture");
		auto* registerHud = GetMenuFrameworkFunction<SKSEMenuFramework::Model::RegisterHudElementFuction>("RegisterHudElement");
		logger::info("menu framework exports: igGetIO {}, igGetBackgroundDrawList_Nil {}, ImDrawList_AddImageQuad {}, ImDrawList_AddPolyline {}, LoadTexture {}, RegisterHudElement {}",
			g_api.getIO != nullptr, g_api.background != nullptr, g_api.addImageQuad != nullptr, g_api.addPolyline != nullptr, g_api.loadTexture != nullptr, registerHud != nullptr);
		if (!g_api.getIO || !g_api.background || !g_api.addImageQuad || !registerHud)
		{
			logger::error("the installed menu framework lacks an export the markers need (listed above); no marker will be drawn - update the Apocrypha Menu Framework");
			return;
		}
		const auto id = registerHud(&Draw);
		g_registered = true;
		logger::info("registered the hit-marker HUD element (id {})", id);
		++g_geoGeneration;
	}

	void RefreshVisibility()
	{
		auto* ui = RE::UI::GetSingleton();
		if (!ui) { return; }
		std::string why;
		if (!ui->IsShowingMenus()) { why = "the HUD is switched off"; }
		else if (ui->IsMenuOpen(RE::LoadingMenu::MENU_NAME)) { why = "Loading Menu"; }
		else if (ui->IsMenuOpen(RE::DialogueMenu::MENU_NAME)) { why = "Dialogue Menu"; }
		else if (!ui->IsCursorHiddenWhenTopmost()) { why = "a menu with a cursor is up"; }
		const bool hidden = !why.empty();
		if (hidden != g_menuHidden.load())
		{
			logger::debug("menus: hit markers {}{}", hidden ? "hidden - " : "allowed", why);
		}
		g_menuHidden = hidden;
		std::scoped_lock lock(g_hiddenWhyLock);
		g_hiddenWhy = hidden ? why : "";
	}

	std::string StateJson()
	{
		const auto geo = detectionmeter::Get();
		std::string why;
		{
			std::scoped_lock lock(g_hiddenWhyLock);
			why = g_hiddenWhy;
		}
		const int art = g_art.load();
		return std::format(
			R"({{"registered":{},"art":"{}","markerSizePx":[{:.0f},{:.0f}],"framesDrawn":{},"markersLastFrame":{},"menuHidden":{},"hiddenWhy":"{}","detectionMeter":{{"artFound":{},"iniFound":{},"radiusX":{:.1f},"radiusY":{:.1f},"centerOffsetX":{:.1f},"centerOffsetY":{:.1f},"flashScale":{:.2f},"flashShift":{:.1f}}},"settings":{{"enabled":{},"showFriendlyHits":{},"duration":{:.2f},"size":{:.2f},"opacity":{:.2f},"color":[{},{},{}],"maxMarkers":{},"ringOffset":{:.0f},"radius":{:.0f},"centerOffsetX":{:.0f},"centerOffsetY":{:.0f}}}}})",
			g_registered.load(), art == kArtDetectionMeter ? "detection-meter" : art == kArtNordic ? "nordic" : art == kArtLines ? "lines" : "not chosen yet", g_markerW.load(), g_markerH.load(), g_framesDrawn.load(),
			g_markersLastFrame.load(), g_menuHidden.load(), why, geo.artFound, geo.iniFound, geo.radiusX, geo.radiusY, geo.centerOffsetX, geo.centerOffsetY, geo.flashScale,
			geo.flashShift, settings::enabled.load(), settings::showFriendlyHits.load(), settings::duration.load(), settings::size.load(), settings::opacity.load(),
			settings::colorR.load(), settings::colorG.load(), settings::colorB.load(), settings::maxMarkers.load(), settings::ringOffset.load(), settings::radius.load(),
			settings::centerOffsetX.load(), settings::centerOffsetY.load());
	}
}

namespace detectionmeter
{
	void Reread()
	{
		Read();
		++g_geoGeneration;
	}
}
