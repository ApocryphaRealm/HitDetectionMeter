# Hit Direction Indicator 1.0.0 - in-game test steps

Profile: Njordlinger Test, with `test Hit Direction Indicator 1.0.0` enabled, and Detection Meter, the SkyALERT packs,
the Apocrypha Menu Framework and DevBench left as they are. Any exterior save; god mode on (`tgm`) so the player
survives the wolves. The log is `Documents\My Games\Skyrim Special Edition\SKSE\HitDirectionIndicator.log`.

1. **Load.** EXPECTED in the log: `Detection Meter art found: ...MaxsuDetectionMeter\Meter_Frame.png and ...Meter_Hostile.png`,
   `Detection Meter INI ...: RadiusX=305.0 RadiusY=305.0 CenterOffsetX=0.0 CenterOffsetY=-48.0 FlashScale=0.25 FlashShift=36.0`,
   `registered the hit-marker HUD element`, `listening for hits on the player (TESHitEvent)`, `Registered "hdi.control" with DevBench`.
2. **DevBench `hdi.control`, no op.** EXPECTED: `hud.registered` true, `hud.detectionMeter.artFound` true and `iniFound` true,
   `hits.hitsSeen` 0.
3. **Injected hits.** `hdi.control op=hit bearing=-90 strength=1`, then `bearing=90`, `bearing=180`, `bearing=0`, a second apart.
   EXPECTED each time: one marker on a ring round the screen centre (the centre 48 px above the middle of the screen,
   about 345 px out at 1080p): -90 at the LEFT, 90 at the RIGHT, 180 at the BOTTOM, 0 at the TOP; the picture is the
   same as Detection Meter's detection markers on this list (its frame with the red hostile fill), turned to lie along
   the ring; it pops slightly larger as it lands and fades out in about 1.5 s. Then the log says `hit markers drawn with
   Detection Meter's art`, and the state's `hud.art` is `detection-meter`.
4. **A real hit from the left.** `player.placeatme 23ABE 1` (a wolf), turn so it attacks from your LEFT, stand still.
   EXPECTED on each bite: a marker at the LEFT of the ring that fades in about 1.5 s; the log has
   `hit from <id> "Wolf": world bearing .., camera yaw .. -> marker at about -90`. Turn the camera while a marker is up:
   EXPECTED: the marker slides round the ring so it keeps pointing at where the wolf stood (it follows the CAMERA, not
   the body).
5. **Several attackers.** `player.placeatme 23ABE 3`, let them surround you. EXPECTED: one marker per wolf at their
   different bearings, each re-brightening on that wolf's next bite; `hdi.control` shows one entry per wolf in `hits.markers`.
6. **Sneaking beside Detection Meter.** Kill the wolves, place one more out of sight, crouch, and let it notice you so a
   Detection Meter marker shows at its bearing, then let it bite you from that same side. EXPECTED: the hit marker and
   the detection marker sit SIDE BY SIDE - the hit marker about 40 px further out on the same bearing - not on top of
   each other, and they look like the same art.
7. **Menus.** Take a hit, open the inventory within a second. EXPECTED: no marker over the inventory; closing it, any
   marker still fading carries on fading (it only ages while drawn).
8. **Settings page.** AMF > Hit Direction Indicator > Settings. EXPECTED: the two booleans are on/off switches; the
   style line says Detection Meter is installed; a D-pad or arrow nudge moves `Distance outside Detection Meter` by 1 px.
   Set it to 80 and inject `op=hit bearing=-90`: EXPECTED the marker sits further out. Press Save: EXPECTED
   `fRingOffset=80` in the mod folder's `SKSE\Plugins\HitDirectionIndicator.ini`. Put it back to 40 and Save.

Send back: `HitDirectionIndicator.log`; the `hdi.control` state after step 5; one capture of step 3 (a marker at the
left) and one of step 6 (hit marker beside a detection marker).
