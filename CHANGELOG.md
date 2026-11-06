# HitDirectionIndicator - changelog

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
- Settings page with switches and precise sliders, eleven languages; DevBench tool hdi.control (state, op=hit bearing strength, op=clear, op=reload).
