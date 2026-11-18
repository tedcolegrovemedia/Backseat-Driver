import CoreLocation
import Foundation

/// Location in the background, reverse geocoded to a road name, plus the
/// direction of travel. Geocoding is throttled: Apple rate-limits it and the
/// road rarely changes faster than every few seconds.
final class RoadTracker: NSObject, ObservableObject {
    @Published private(set) var road: RoadNames.Result?
    @Published private(set) var bearing: String?
    @Published private(set) var isDay = false
    @Published private(set) var authorization: CLAuthorizationStatus = .notDetermined
    @Published private(set) var lastFix: Date?
    var onChange: (() -> Void)?

    private let manager = CLLocationManager()
    private let geocoder = CLGeocoder()
    private var lastGeocode: (place: CLLocation, at: Date)?
    private var geocoding = false
    private var lastCourseAt = Date.distantPast   // last time GPS gave a travel direction

    override init() {
        super.init()
        manager.delegate = self
        manager.desiredAccuracy = kCLLocationAccuracyBestForNavigation
        manager.activityType = .automotiveNavigation
        manager.distanceFilter = 10
        manager.pausesLocationUpdatesAutomatically = false
        authorization = manager.authorizationStatus
    }

    func start() {
        switch manager.authorizationStatus {
        case .notDetermined: manager.requestWhenInUseAuthorization()
        case .authorizedWhenInUse: manager.requestAlwaysAuthorization(); begin()
        case .authorizedAlways: begin()
        default: break
        }
    }

    private func begin() {
        manager.allowsBackgroundLocationUpdates = true
        manager.showsBackgroundLocationIndicator = true
        manager.startUpdatingLocation()
        if CLLocationManager.headingAvailable() {
            manager.headingFilter = 10
            manager.startUpdatingHeading()
        }
    }

    private func geocodeIfNeeded(_ loc: CLLocation) {
        guard !geocoding else { return }
        if let last = lastGeocode,
           loc.distance(from: last.place) < 60, Date().timeIntervalSince(last.at) < 15 { return }
        geocoding = true
        geocoder.reverseGeocodeLocation(loc) { [weak self] places, _ in
            DispatchQueue.main.async {
                guard let self else { return }
                self.geocoding = false
                self.lastGeocode = (loc, Date())
                if let street = places?.first?.thoroughfare, !street.isEmpty {
                    let r = RoadNames.normalize(street)
                    if r != self.road { self.road = r; self.onChange?() }
                }
            }
        }
    }
}

extension RoadTracker: CLLocationManagerDelegate {
    func locationManagerDidChangeAuthorization(_ m: CLLocationManager) {
        authorization = m.authorizationStatus
        start()
    }

    func locationManager(_ m: CLLocationManager, didUpdateLocations locs: [CLLocation]) {
        guard let loc = locs.last, loc.horizontalAccuracy >= 0, loc.horizontalAccuracy < 50 else { return }
        lastFix = loc.timestamp
        var changed = false
        // GPS course is the real direction of travel, but only exists while
        // moving (> ~7 km/h). Some fixes report no course accuracy; accept
        // those once clearly moving.
        let goodAccuracy = loc.courseAccuracy >= 0 ? loc.courseAccuracy < 30 : loc.speed > 4
        if loc.speed > 2, loc.course >= 0, goodAccuracy {
            lastCourseAt = Date()
            let b = RoadNames.bearing(loc.course)
            if b != bearing { bearing = b; changed = true }
        }
        let day = Sun.isDay(latitude: loc.coordinate.latitude, longitude: loc.coordinate.longitude)
        if day != isDay { isDay = day; changed = true }
        if changed { onChange?() }
        geocodeIfNeeded(loc)
    }

    /// Compass fallback while stopped: used only when GPS hasn't given a travel
    /// direction in the last 2 minutes (parked, or indoors). Once the car
    /// moves, GPS course takes over and is held through stops.
    func locationManager(_ m: CLLocationManager, didUpdateHeading h: CLHeading) {
        guard Date().timeIntervalSince(lastCourseAt) > 120, h.headingAccuracy >= 0 else { return }
        let deg = h.trueHeading >= 0 ? h.trueHeading : h.magneticHeading
        let b = RoadNames.bearing(deg)
        if b != bearing { bearing = b; onChange?() }
    }
}
