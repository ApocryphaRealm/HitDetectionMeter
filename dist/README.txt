Hit Direction Indicator
Version 1.0.0

When something hits you, a marker on a ring round the middle of the screen shows which way the hit came from, and fades in about a second and a half. The marker keeps pointing at where the attacker stood while you turn the camera. Each attacker has one marker; a new hit from it brightens it again. Power attacks show brightest, ordinary hits a little dimmer, hits you blocked fainter still.

With Detection Meter installed, the hit markers wear Detection Meter's own art, whichever art pack you chose for it (SkyALERT and the like), and sit on a ring just outside its ring, so they read as one HUD and a hit taken while sneaking sits beside a detection marker, not on top of it. Its ring's size and centre are read from MaxsuDetectionMeter.ini. Without Detection Meter, the markers are a carved stone and iron slab with a blood-red knotwork channel and an arrow rune.

Hits you deal yourself never show. Hits from followers and anyone not hostile to you can be left out (bShowFriendlyHits=0).

Settings: a page in the Apocrypha Menu Framework, and the INI, SKSE\Plugins\HitDirectionIndicator.ini - on or off, friends' hits, fade time, size, opacity, tint, markers at once, and the ring's distance outside Detection Meter's (40 pixels by default).

Requires SKSE, Address Library, and the Apocrypha Menu Framework (it draws the markers). Detection Meter is optional. Skyrim SE 1.5.97, AE 1.6.1170, and 1.7.99 / 1.7.104.
