import Foundation

/// Day or night from the sun's elevation at the phone's position. The board
/// has no clock or light sensor, so the phone decides.
enum Sun {
    /// Sun elevation in degrees (simplified NOAA/USNO formula, about ±0.5°).
    static func elevation(latitude: Double, longitude: Double, at date: Date = Date()) -> Double {
        let rad = Double.pi / 180
        let d = date.timeIntervalSince1970 / 86400.0 + 2440587.5 - 2451545.0   // days since J2000
        let g = (357.529 + 0.98560028 * d) * rad
        let q = 280.459 + 0.98564736 * d
        let L = (q + 1.915 * sin(g) + 0.020 * sin(2 * g)) * rad
        let e = (23.439 - 0.00000036 * d) * rad
        let ra = atan2(cos(e) * sin(L), cos(L))
        let decl = asin(sin(e) * sin(L))
        let gmstHours = 18.697374558 + 24.06570982441908 * d
        let lha = (gmstHours * 15 + longitude) * rad - ra
        let lat = latitude * rad
        return asin(sin(lat) * sin(decl) + cos(lat) * cos(decl) * cos(lha)) / rad
    }

    /// Day once the sun is above the horizon (refraction-corrected sunrise).
    static func isDay(latitude: Double, longitude: Double, at date: Date = Date()) -> Bool {
        elevation(latitude: latitude, longitude: longitude, at: date) > -0.833
    }
}
