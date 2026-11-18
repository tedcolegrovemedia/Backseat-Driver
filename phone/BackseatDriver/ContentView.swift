import CoreLocation
import SwiftUI

struct ContentView: View {
    @EnvironmentObject var companion: Companion
    var body: some View {
        StatusView(companion: companion, link: companion.link, road: companion.road, spotify: companion.spotify)
    }
}

struct StatusView: View {
    @ObservedObject var companion: Companion
    @ObservedObject var link: BLELink
    @ObservedObject var road: RoadTracker
    @ObservedObject var spotify: Spotify

    var body: some View {
        NavigationStack {
            List {
                Section("Headrest display") {
                    row("Bluetooth", link.state.rawValue, ok: link.state == .connected)
                    row("Last sent", companion.lastSent, ok: nil)
                }
                Section("Road") {
                    let (name, kind, dir) = companion.currentRoad()
                    row("Road", name ?? "Finding the road...", ok: name != nil)
                    row(kind == .signed ? "Signed direction" : "Heading", dir.isEmpty ? "-" : dir, ok: nil)
                    row("Look", road.isDay ? "Day" : "Night", ok: nil)
                    row("Last GPS fix", road.lastFix.map { $0.formatted(date: .omitted, time: .standard) } ?? "none", ok: road.lastFix != nil)
                }
                Section("Spotify") {
                    row("Account", spotify.status.text, ok: spotify.status == .connected)
                    if spotify.status == .connected {
                        row("Now playing", spotify.song.map { "\($0.title) - \($0.artist)" }
                            ?? (spotify.note.isEmpty ? "-" : spotify.note), ok: nil)
                        Button("Disconnect Spotify", role: .destructive) { spotify.signOut() }
                    } else {
                        Button("Connect Spotify") { spotify.signIn() }
                            .disabled(spotify.status == .signingIn)
                    }
                }
                Section("Location permission") {
                    row("Access", authText, ok: road.authorization == .authorizedAlways)
                    if road.authorization != .authorizedAlways {
                        Text("Set Location to \"Always\" so the road keeps updating while Waze is in front or the phone is locked.")
                            .font(.footnote).foregroundStyle(.secondary)
                        Button("Open Settings") {
                            if let url = URL(string: UIApplication.openSettingsURLString) { UIApplication.shared.open(url) }
                        }
                    }
                }
            }
            .navigationTitle("Backseat Driver")
        }
    }

    private var authText: String {
        switch road.authorization {
        case .authorizedAlways: return "Always"
        case .authorizedWhenInUse: return "While using the app"
        case .denied, .restricted: return "Off"
        default: return "Not asked yet"
        }
    }

    private func row(_ label: String, _ value: String, ok: Bool?) -> some View {
        HStack {
            Text(label)
            Spacer()
            Text(value).foregroundStyle(ok == false ? .orange : .secondary).multilineTextAlignment(.trailing)
        }
    }
}
