import SwiftUI

@main
struct BackseatDriverApp: App {
    @StateObject private var companion = Companion()
    var body: some Scene {
        WindowGroup { ContentView().environmentObject(companion) }
    }
}

/// Decides what to send. Sends on every change, and repeats the current state
/// every 5 s so the display's freshness timers only fire if the phone stops.
final class Companion: ObservableObject {
    let link = BLELink()
    let road = RoadTracker()
    let spotify = Spotify()
    @Published private(set) var lastSent: String = "nothing yet"
    private var timer: Timer?

    init() {
        link.onReady = { [weak self] in self?.sendAll() }
        road.onChange = { [weak self] in self?.sendAll() }
        spotify.onChange = { [weak self] in self?.sendAll() }
        road.start()
        timer = Timer.scheduledTimer(withTimeInterval: 5, repeats: true) { [weak self] _ in self?.sendAll() }
    }

    func sendAll() {
        guard link.state == .connected else { return }
        link.send(LinkMessage.light(day: road.isDay))
        let (name, kind, dir) = currentRoad()
        if let name {
            link.send(LinkMessage.road(name: name, kind: kind, direction: dir))
            lastSent = "\(name) \(dir)".trimmingCharacters(in: .whitespaces)
        }
        if let song = spotify.song {
            link.send(LinkMessage.song(title: song.title, artist: song.artist, playing: true))
        } else {
            link.send(LinkMessage.noSong)
        }
    }

    /// Signed direction only when the road name carried one; otherwise the
    /// compass bearing, shown on the display as "heading ...".
    func currentRoad() -> (String?, LinkMessage.DirectionKind, String) {
        guard let r = road.road else { return (nil, .none, "") }
        if let s = r.signedDirection { return (r.name, .signed, s) }
        if let b = road.bearing { return (r.name, .bearing, b) }
        return (r.name, .none, "")
    }
}
