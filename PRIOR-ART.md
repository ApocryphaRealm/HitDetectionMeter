# Prior art for Hit Detection Meter (searched 2026-11-06, rule 24)

Wanted: when an enemy hits the player, a mark at the screen edge shows which way the hit came from, on SE 1.5.97, AE and 1.7.

Searched Nexus (Skyrim Special Edition, 2026-11-06, by the coordinating session): hit direction, damage direction,
damage indicator, hit indicator and related terms. No Skyrim SE hit-direction HUD mod was found. The nearest:

| Mod | Where | What it does | Why it is not this |
|---|---|---|---|
| Detection Meter (Maxsu) | Nexus 63057, github max-su-2019/MaxsuDetectionMeter (MIT) | A frame-and-fill marker at the bearing of each NPC detecting you while sneaking, round the screen centre | Detection, not hits, and only while sneaking. This mod draws beside it with its art (the owner's ask, 2026-11-06) |
| SkyALERT - Detection Meter Redesign | Nexus 72045 | An art pack for Detection Meter | Art only |
| a HUD layer framework | Nexus 58728 | A framework for HUD layers | No hit direction |

Searched GitHub (`gh search repos`, 2026-11-06): "skyrim hit direction", "damage direction indicator skse",
"skse hit indicator" - no results. "DetectionMeter" found max-su-2019/MaxsuDetectionMeter and its forks.

Our own Oblivion Remastered HUD Position Manager (github ApocryphaRealm/HUDPositionManagerOR, src/Hud.cpp) already has a
damage direction indicator; its logic (a flash per hit with angle, time and strength, linear fade over 1.5 s, oldest
dropped past the cap) is ported here, with the owner's leave.
