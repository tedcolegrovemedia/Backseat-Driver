# Integration notes and blockers

Cars are shown by name only (no pictures).

Status as of November 16, 2026: the board runs the shared app code, and the iPhone app (`phone/`) connects over
BLE and sends a live road name, bearing and day/night look. Road names come from Apple reverse geocoding for now;
the OSM map matching below is still the plan for real signed directions. Spotify now-playing works (since November 17).
Items marked **verify** are open questions, not findings.

## Where everything runs

```
iPhone companion app                         ESP32 display
-----------------------------------          ------------------------
GPS (Core Location, background)
  -> map-match against an offline   --BLE-->  bsd_feed_set_road()
     Lehigh Valley road extract      GATT     bsd_feed_set_song()
Spotify Web API (phone's internet)  ------->  bsd_feed_set_link()
                                              UI and game, fully offline
```

- **The ESP32 has no internet.** BLE to the phone does not give it any. All map and Spotify requests run on the phone.
- The board receives only small, already-decided facts: road name, direction kind, direction, song title and artist, and day or night (the phone computes this from local sunset). The `bsd_feed_*` API in `app/include/bsd_feed.h` is that contract. The demo source writes the same calls.

## Road name and signed direction (top priority, biggest unknown)

GPS gives a position and a course (compass bearing). It does **not** give "Route 22 West."

- **Reverse geocoding** (Apple `CLGeocoder`) returns a street name (`thoroughfare`). It is rate-limited, needs internet, and gives no signed direction. It may return "Lehigh Valley Thruway" where we want "Route 22". Usable as a fallback only.
- **Recommended: offline map matching against OpenStreetMap.** Ship a small Lehigh Valley extract in the phone app.
  - OSM ways carry `ref` (for example `US 22`, `PA 378`, `I 78`) and `name`. That lets the app say "Route 22" instead of a local street name.
  - US and state route **relations** in OSM often tag directional members with roles `north`/`south`/`east`/`west`, especially on divided highways where each carriageway is its own one-way. That is the signed direction itself, not a guess from bearing. **Verify** how complete this is for US 22, PA 378, I-78, and PA 309 in the extract.
  - For undivided roads without roles, derive the direction from the route's signed axis (E–W or N–S) and the direction of travel along the relation's member order. Only show it as signed when that is unambiguous. Otherwise send `BSD_DIR_BEARING` ("heading northwest") or `BSD_DIR_NONE`.
- **Parallel roads and interchanges:** require several consistent fixes and a course that matches the way's direction before switching roads. Hold the last road through brief ambiguity. The display already dims after 15 s without updates and shows "Finding the road..." after 90 s.
- This works without a Waze route and without internet.

## iPhone companion app (verify all of these)

- **Background location while Waze is in front or the phone is locked:** needs the `location` background mode plus `allowsBackgroundLocationUpdates`. Test on a real drive.
- **BLE:** phone as central, ESP32 as peripheral (NimBLE). Needs the `bluetooth-central` background mode and state restoration so iOS reconnects after the board power-cycles. Test reconnect when the car turns off and on.
- **Build and install:** a free Apple ID can sideload from Xcode, but the profile expires after **7 days**. Install no earlier than about Wednesday for a Friday demo. A paid developer account removes the limit.
- **Other apps' playback:** iOS does not expose Spotify's now-playing metadata to third-party apps. `MPNowPlayingInfoCenter` covers only your own app's playback, and `MPMusicPlayerController` covers only Apple Music. Spotify has to come from the Web API.

## Spotify (lowest priority; verify before building)

- The endpoint is `GET /v1/me/player/currently-playing`. It needs OAuth (PKCE) with `user-read-currently-playing`, run on the phone and polled every 5–10 s over cellular.
- **Verify on developer.spotify.com before writing code:** current development-mode rules (Premium requirement for the app owner, allowed-user limits), rate limits, and whether policy allows this use.
- Account-level "currently playing" might be another device, such as a home speaker. Check the `device` field from `/me/player` and show the song only when the phone or car is the active device.

## Waze

Deferred. The current-road feature does not depend on Waze. The Transport SDK ends November 1, 2026, and Navigation Connect lists no simple next-turn feed.

## What the simulator does not prove

Physical touch through the enclosure, RGB panel timing and tearing, PSRAM bandwidth, power draw from a car USB port, sunlight readability, and BLE reliability.
