# iPhone companion app

Sends the current road, direction and day/night look to the headrest display over Bluetooth LE.
Working end to end since November 16, 2026 (Spotify since November 17): the phone finds the board within about 1.5 s of it booting,
and the first message switches the screen from DEMO DATA to live.

## Build and install

```bash
cd phone && xcodegen
xcodebuild -project BackseatDriver.xcodeproj -scheme BackseatDriver -destination 'generic/platform=iOS' \
  -derivedDataPath ~/.cache/backseat-phone-build -allowProvisioningUpdates build
xcrun devicectl device install app --device <UDID> ~/.cache/backseat-phone-build/Build/Products/Debug-iphoneos/BackseatDriver.app
```

The app is signed with a paid developer team, so there is no 7-day expiry.
Set Location to **Always** in Settings so the road keeps updating while Waze is in front or the phone is locked.

## How it decides what to send

| Piece | Source | Notes |
|---|---|---|
| Road name | `CLGeocoder` reverse geocoding (`thoroughfare`) | Throttled to every 60 m or 15 s. `RoadNames` turns "US-22" into "Route 22" and "I-78" into "Interstate 78". |
| Signed direction | Only when the geocoded name ends in a direction ("US-22 W") | **Verify on a drive** whether Apple ever returns this. Otherwise it is sent as a bearing. |
| Bearing | GPS course above about 7 km/h; the phone's compass when GPS hasn't given a course for 2 minutes | 8 compass points, shown as "heading ...". When parked, the compass reading follows the phone, not the car. |
| Day / night | Sun elevation at the phone's position (`Sun.swift`) | Day once the sun is above the horizon |
| Song | Spotify Web API `GET /v1/me/player`, every 6 s | PKCE sign-in, no client secret. Spotify allows only HTTPS or loopback redirects, so `Spotify.swift` listens on `http://127.0.0.1:8571/callback` while the sign-in sheet is open. The refresh token is kept in the Keychain. The song shows only when the playing device is a smartphone or car, and "no song" is sent when paused. Backs off on HTTP 429. |

The message format is in `app/include/bsd_link.h`, and `LinkMessage.swift` mirrors it. Text is reduced to printable ASCII
on the phone because the display fonts are ASCII only. The phone repeats the road every 5 s so the display's
"dim after 15 s" and "finding the road after 90 s" timers fire only when the phone really stops.

## Spotify app

Create your own app at [developer.spotify.com/dashboard](https://developer.spotify.com/dashboard) with redirect URI
`http://127.0.0.1:8571/callback` and Web API access, then paste its Client ID into `Spotify.swift` (`clientID`).
The Client ID isn't a secret (PKCE, no client secret). Development mode allows 5 users and needs the owner to have Premium.
