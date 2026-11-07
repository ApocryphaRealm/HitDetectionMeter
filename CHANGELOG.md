# HitDetectionMeter - changelog

Written as changes happen, not reconstructed afterwards (rule 61). Each version carries its
**version-ledger status**, so this file cannot quietly claim more than the ledger does:

* **working** - observed running in game
* **untested** - built and packaged, not yet confirmed
* **failed** - built but crashed or malfunctioned; the number was reclaimed
* **scratch** - a hypothesis-test build that never held a real number

## 1.0.0 - 2026-11-06 - untested

### Added
- When something hits the player, a marker on a ring round the screen centre shows which way the hit came from (TESHitEvent on the player; the aggressor, or the shooter of a projectile). It keeps pointing at where the attacker stood as the camera turns (camera yaw, as Dragon's Eye Minimap's compass ring), fades over fDuration (1.5 s) with a short pop as it lands, and only ages while it is drawn. One marker per attacker; past iMaxMarkers (8) the oldest goes. Power attacks draw at full opacity, bashes at 0.85, ordinary hits at 0.7, blocked hits at six tenths of that. Self-hits never show; bShowFriendlyHits=0 leaves out followers and anyone not hostile.
- With Detection Meter (Nexus 63057) installed, the markers wear its Meter_Frame.png and Meter_Hostile.png, whichever art pack wins, on its ring (RadiusX/RadiusY/CenterOffsetX/CenterOffsetY from MaxsuDetectionMeter.ini) moved fRingOffset (40) pixels out, sized for the screen as Detection Meter sizes its own. Without it: a Nordic carved-stone marker drawn for this mod, on fRadius (305) + 40 with Detection Meter's default centre.
- Drawn through the Apocrypha Menu Framework's HUD-element API; hidden while the game is paused, in dialogue, in a menu with a cursor, or with the HUD off.
- Settings page with switches and precise sliders, eleven languages; DevBench tool hdm.control (state, op=hit bearing strength, op=clear, op=reload).

### Fixed
- Real hits are placed against where the camera looks, read from the world camera's view direction (NiCamera world rotation). PlayerCamera's RUNTIME_DATA2 yaw, used before, read 186 on every hit even after the camera was turned 90 degrees. Each hit logs every yaw source at debug (NiCamera, third-person heading + free rotation, RUNTIME_DATA2, body).
- Hit markers are sized exactly as Detection Meter sizes its own markers: the art PNG's pixels times screen width / 1920 and height / 1080, cut to whole pixels as Detection Meter's renderer does (it has no INI size key). fSize multiplies that, 1.0 = Detection Meter's size. The size is logged once ('hit marker size W x H px') and shown in hdm.control as hud.markerSizePx.
### Changed
- The hit ring now sits INSIDE Detection Meter's ring, closer to the character (the project owner, 2026-11-07): fRingOffset defaults to -40 (305 - 40 = 265 px at 1080p), in the INI and the compiled default alike; the setting runs -300..300 and is labelled 'Distance from Detection Meter's ring (negative = closer to the character)'. Without Detection Meter the fallback ring is 40 px closer too.
- Renamed Hit Direction Indicator to Hit Detection Meter before release (the project owner, 2026-11-07): HitDetectionMeter.dll / .ini / .log, HitDetectionMeter_<language>.txt translations, the AMF page, the DevBench tool hdm.control, the FOMOD and the repo folder.