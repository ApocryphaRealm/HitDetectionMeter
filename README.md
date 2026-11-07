# Hit Detection Meter

An SKSE plugin for Skyrim SE 1.5.97, AE 1.6.1170 and 1.7.99 / 1.7.104. When something hits the player, a marker on a ring
round the screen centre shows which way the hit came from, and fades.

* **Hits:** a `TESHitEvent` sink. The target is the player; the aggressor is the cause when it is an actor, or the
  shooter of the projectile that is the cause. Self-hits are ignored; `bShowFriendlyHits=0` ignores followers and
  anyone not hostile. One marker per aggressor; a new hit refreshes it.
* **Direction:** the bearing from the player to where the aggressor stood, against the camera yaw
  (`PlayerCamera` `RUNTIME_DATA2.yaw`), every frame - the marker keeps pointing at the source as the view turns.
* **Fade:** linear over `fDuration` (1.5 s), opacity = strength x (1 - age), with a short pop as it lands. Ported from
  our Oblivion Remastered HUD Position Manager's damage direction indicator.
* **Drawing:** an Apocrypha Menu Framework HUD element (render thread, no game locks), `ImDrawList_AddImageQuad`.
* **Detection Meter (Nexus 63057):** when installed, its `Meter_Frame.png` + `Meter_Hostile.png` (whichever art pack
  wins) are drawn, on its own ring from `MaxsuDetectionMeter.ini` moved by `fRingOffset` (-40 px: 40 px in, closer to the character). Otherwise the Nordic
  art from `tools/gen-nordic-art.py`.
* **Observability:** DevBench tool `hdm.control` - state, `op=hit bearing=<deg> strength=<0..1>`, `op=clear`, `op=reload`.

## Building

Visual Studio 2022 with the C++ workload, and `VCPKG_ROOT` pointing at a vcpkg checkout.

* SE 1.5.97 / AE 1.6.x: `configure.bat`, then `build.bat` (CommonLibSSE-NG 3.7, `build/relwithdebinfo-se-only`).
* Skyrim 1.7.x: `configure17.bat`, then `build17.bat` (CommonLibSSE-NG 7.2, `build/relwithdebinfo-17`).
* Art: `py -3 tools/gen-nordic-art.py [--preview [Meter_Frame.png Meter_Hostile.png]]` (Pillow, numpy).
* Translations: `py -3 tools/make_translations.py`.

GPL-3.0-or-later (`LICENSE`, `NOTICE.md`, `THIRD_PARTY_NOTICES.md`).
