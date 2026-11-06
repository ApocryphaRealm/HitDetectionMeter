#include "PCH.h"

#include "DevBenchTool.h"

#include "DevBench/DevBenchAPI.h"
#include "Indicator.h"
#include "Settings.h"
#include "utils/Logger.h"
#include "utils/Strings.h"

#include <chrono>
#include <cstdlib>
#include <format>
#include <future>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

namespace DevBenchTool
{
	namespace
	{
		// DevBench calls on its listener thread; anything that touches the game runs on the main thread.
		template <class F>
		std::string OnMain(F a_fn)
		{
			auto* tasks = SKSE::GetTaskInterface();
			if (!tasks) { return R"({"ok":false,"error":"no task interface"})"; }
			auto promise = std::make_shared<std::promise<std::string>>();
			auto future = promise->get_future();
			tasks->AddTask([promise, a_fn]() { promise->set_value(a_fn()); });
			if (future.wait_for(std::chrono::seconds(5)) != std::future_status::ready) { return R"({"ok":false,"error":"the main thread did not answer in 5 seconds"})"; }
			return future.get();
		}

		// The op's name: the string value of "op".
		std::string OpFrom(std::string_view a_args)
		{
			const auto at = a_args.find("\"op\"");
			if (at == std::string_view::npos) { return {}; }
			const auto open = a_args.find('"', a_args.find(':', at + 4) + 1);
			if (open == std::string_view::npos) { return {}; }
			const auto close = a_args.find('"', open + 1);
			if (close == std::string_view::npos) { return {}; }
			return std::string(a_args.substr(open + 1, close - open - 1));
		}

		// A number field ("bearing":-90, "bearing":"-90"). Missing: nothing.
		std::optional<double> NumberFrom(std::string_view a_args, const char* a_name)
		{
			const std::string key = std::string("\"") + a_name + "\"";
			const auto at = a_args.find(key);
			if (at == std::string_view::npos) { return std::nullopt; }
			auto p = a_args.find(':', at + key.size());
			if (p == std::string_view::npos) { return std::nullopt; }
			++p;
			while (p < a_args.size() && (a_args[p] == ' ' || a_args[p] == '"')) { ++p; }
			const std::string rest(a_args.substr(p, 24));
			char* end = nullptr;
			const double v = std::strtod(rest.c_str(), &end);
			if (end == rest.c_str()) { return std::nullopt; }
			return v;
		}

		std::string State()
		{
			return std::format(R"({{"ok":true,"hud":{},"hits":{},"strings":{}}})", hud::StateJson(), hits::StateJson(), strings::StatusJson());
		}

		void ControlTool(void*, const char* a_argsJson, void* a_sink, DevBenchAPI::WriteFn a_write)
		{
			const std::string args = a_argsJson ? a_argsJson : "";
			const std::string op = OpFrom(args);
			std::string out;
			if (op == "hit")
			{
				// a hit from this bearing relative to the camera (0 ahead, 90 right, 180 behind, -90 left), drawn like a real one
				const double bearing = NumberFrom(args, "bearing").value_or(0.0);
				const double strength = NumberFrom(args, "strength").value_or(1.0);
				hits::Inject(static_cast<float>(bearing), static_cast<float>(strength));
				out = std::format(R"({{"ok":true,"op":"hit","bearing":{:.1f},"strength":{:.2f}}})", bearing, strength);
			}
			else if (op == "clear")
			{
				hits::Clear();
				out = R"({"ok":true,"op":"clear"})";
			}
			else if (op == "reload")
			{
				out = OnMain([]() {
					const bool ok = settings::Reload();
					detectionmeter::Reread();
					return std::format(R"({{"ok":{},"op":"reload"}})", ok ? "true" : "false");
				});
			}
			else { out = State(); }
			if (a_write) { a_write(a_sink, out.c_str()); }
		}
	}

	void Init(bool a_lastAttempt)
	{
		static bool registered = false;
		if (registered) { return; }

		DevBenchAPI::IDevBenchInterface001* devBench = DevBenchAPI::GetDevBenchInterface001();
		if (!devBench)
		{
			if (a_lastAttempt) { logger::info("DevBench not detected; skipping the \"hdi.control\" tool"); }
			else { logger::debug("DevBench not detected yet; will retry at the next message"); }
			return;
		}

		constexpr const char* descriptor =
			"{"
			"\"description\":\"Hit Direction Indicator live state and driver. No op: the HUD element (registered, art in use, frames drawn, markers drawn last frame, "
			"what hides it), Detection Meter's art and ring as read, the settings, hit counters (seen, shown, ignored and why), the markers up and the last hit. "
			"op=hit bearing=<degrees, 0 ahead, 90 right, 180 behind, -90 left> strength=<0..1>: draws a marker as a real hit from that side would. "
			"op=clear removes every marker. op=reload re-reads the INI and Detection Meter's files.\","
			"\"inputSchema\":{\"type\":\"object\",\"properties\":{\"op\":{\"type\":\"string\"},\"bearing\":{\"type\":\"number\"},\"strength\":{\"type\":\"number\"}},\"additionalProperties\":true},"
			"\"readOnly\":false"
			"}";

		if (devBench->RegisterTool("hdi.control", descriptor, &ControlTool, nullptr))
		{
			logger::info("Registered \"hdi.control\" with DevBench (build {})", devBench->GetBuildNumber());
			registered = true;
		}
	}
}
