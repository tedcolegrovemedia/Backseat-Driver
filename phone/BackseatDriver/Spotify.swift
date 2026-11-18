import AuthenticationServices
import CryptoKit
import Foundation
import Network
import Security
import UIKit

/// Spotify "what's playing" for the headrest display.
///
/// Sign-in is OAuth with PKCE (no client secret on the phone). Spotify only
/// accepts HTTPS or loopback redirect URIs, so the app listens on
/// http://127.0.0.1:8571/callback while the sign-in sheet is open and picks the
/// code up there. The refresh token lives in the Keychain.
final class Spotify: NSObject, ObservableObject {
    static let clientID = "YOUR_SPOTIFY_CLIENT_ID"  // from developer.spotify.com/dashboard
    static let port: UInt16 = 8571
    static let redirectURI = "http://127.0.0.1:8571/callback"
    static let scopes = "user-read-currently-playing user-read-playback-state"

    struct Song: Equatable { var title: String; var artist: String }

    enum Status: Equatable {
        case signedOut, signingIn, connected, error(String)
        var text: String {
            switch self {
            case .signedOut: return "Not connected"
            case .signingIn: return "Signing in..."
            case .connected: return "Connected"
            case .error(let e): return e
            }
        }
    }

    @Published private(set) var status: Status = .signedOut
    @Published private(set) var song: Song?          // nil = nothing playing on this phone
    @Published private(set) var note = ""            // why there is no song, for the status screen
    var onChange: (() -> Void)?

    private var accessToken: String?
    private var tokenExpiry = Date.distantPast
    private var verifier = ""
    private var state = ""
    private var session: ASWebAuthenticationSession?
    private var listener: NWListener?
    private var pollTimer: Timer?
    private var backoffUntil = Date.distantPast
    private var polling = false

    override init() {
        super.init()
        if Keychain.get("spotify.refresh") != nil { status = .connected; startPolling() }
    }

    // MARK: sign-in

    func signIn() {
        verifier = Self.randomString(64)
        state = Self.randomString(16)
        let challenge = Data(SHA256.hash(data: Data(verifier.utf8))).base64URL
        var c = URLComponents(string: "https://accounts.spotify.com/authorize")!
        c.queryItems = [
            .init(name: "client_id", value: Self.clientID),
            .init(name: "response_type", value: "code"),
            .init(name: "redirect_uri", value: Self.redirectURI),
            .init(name: "code_challenge_method", value: "S256"),
            .init(name: "code_challenge", value: challenge),
            .init(name: "scope", value: Self.scopes),
            .init(name: "state", value: state),
        ]
        guard startListener() else { status = .error("Couldn't open the sign-in listener"); return }
        status = .signingIn
        // The callback scheme never fires; the loopback listener ends the session.
        let s = ASWebAuthenticationSession(url: c.url!, callbackURLScheme: "backseatdriver") { [weak self] _, error in
            DispatchQueue.main.async {
                guard let self else { return }
                self.stopListener()
                if self.status == .signingIn { self.status = .signedOut }
            }
        }
        s.presentationContextProvider = self
        s.prefersEphemeralWebBrowserSession = false
        session = s
        s.start()
    }

    func signOut() {
        Keychain.delete("spotify.refresh")
        accessToken = nil
        pollTimer?.invalidate()
        pollTimer = nil
        status = .signedOut
        setSong(nil, note: "")
    }

    private func startListener() -> Bool {
        stopListener()
        let params = NWParameters.tcp
        params.requiredLocalEndpoint = .hostPort(host: "127.0.0.1", port: NWEndpoint.Port(rawValue: Self.port)!)
        params.allowLocalEndpointReuse = true
        guard let l = try? NWListener(using: params) else { return false }
        l.newConnectionHandler = { [weak self] conn in self?.handle(conn) }
        l.start(queue: .main)
        listener = l
        return true
    }

    private func stopListener() {
        listener?.cancel()
        listener = nil
    }

    private func handle(_ conn: NWConnection) {
        conn.start(queue: .main)
        conn.receive(minimumIncompleteLength: 1, maximumLength: 8192) { [weak self] data, _, _, _ in
            guard let self, let data, let req = String(data: data, encoding: .utf8),
                  let line = req.split(separator: "\r\n").first else { conn.cancel(); return }
            let parts = line.split(separator: " ")
            let target = parts.count > 1 ? String(parts[1]) : ""
            let items = URLComponents(string: "http://127.0.0.1" + target)?.queryItems ?? []
            let code = items.first { $0.name == "code" }?.value
            let gotState = items.first { $0.name == "state" }?.value
            let ok = code != nil && gotState == self.state
            let body = ok ? "Spotify is connected. You can close this page."
                          : "Spotify sign-in didn't finish. Go back to the app and try again."
            let html = "<html><body style='font:20px -apple-system;padding:40px'>\(body)</body></html>"
            let resp = "HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\nContent-Length: \(html.utf8.count)\r\nConnection: close\r\n\r\n\(html)"
            conn.send(content: Data(resp.utf8), completion: .contentProcessed { _ in conn.cancel() })
            guard target.hasPrefix("/callback") else { return }
            self.session?.cancel()
            self.stopListener()
            if let code, ok { self.exchange(code: code) } else { self.status = .error("Sign-in was cancelled") }
        }
    }

    // MARK: tokens

    private func exchange(code: String) {
        tokenRequest([
            "grant_type": "authorization_code", "code": code, "redirect_uri": Self.redirectURI,
            "client_id": Self.clientID, "code_verifier": verifier,
        ]) { [weak self] ok in
            guard let self else { return }
            if ok { self.status = .connected; self.startPolling() }
            else { self.status = .error("Spotify didn't accept the sign-in") }
        }
    }

    private func refresh(_ done: @escaping (Bool) -> Void) {
        guard let rt = Keychain.get("spotify.refresh") else { done(false); return }
        tokenRequest(["grant_type": "refresh_token", "refresh_token": rt, "client_id": Self.clientID], done)
    }

    private func tokenRequest(_ form: [String: String], _ done: @escaping (Bool) -> Void) {
        var r = URLRequest(url: URL(string: "https://accounts.spotify.com/api/token")!)
        r.httpMethod = "POST"
        r.setValue("application/x-www-form-urlencoded", forHTTPHeaderField: "Content-Type")
        r.httpBody = form.map { "\($0.key)=\($0.value.formEncoded)" }.joined(separator: "&").data(using: .utf8)
        URLSession.shared.dataTask(with: r) { [weak self] data, resp, _ in
            DispatchQueue.main.async {
                guard let self else { return }
                let code = (resp as? HTTPURLResponse)?.statusCode ?? 0
                guard code == 200, let data,
                      let j = try? JSONSerialization.jsonObject(with: data) as? [String: Any],
                      let at = j["access_token"] as? String else {
                    if code == 400 { self.signOut(); self.status = .error("Spotify sign-in expired; connect again") }
                    done(false); return
                }
                self.accessToken = at
                self.tokenExpiry = Date().addingTimeInterval(TimeInterval((j["expires_in"] as? Int) ?? 3600) - 60)
                if let rt = j["refresh_token"] as? String { Keychain.set("spotify.refresh", rt) }
                done(true)
            }
        }.resume()
    }

    // MARK: now playing

    private func startPolling() {
        pollTimer?.invalidate()
        pollTimer = Timer.scheduledTimer(withTimeInterval: 6, repeats: true) { [weak self] _ in self?.poll() }
        poll()
    }

    private func poll() {
        guard !polling, Date() >= backoffUntil, Keychain.get("spotify.refresh") != nil else { return }
        if accessToken == nil || Date() >= tokenExpiry {
            polling = true
            refresh { [weak self] ok in self?.polling = false; if ok { self?.poll() } }
            return
        }
        polling = true
        var r = URLRequest(url: URL(string: "https://api.spotify.com/v1/me/player?additional_types=episode")!)
        r.setValue("Bearer \(accessToken!)", forHTTPHeaderField: "Authorization")
        URLSession.shared.dataTask(with: r) { [weak self] data, resp, _ in
            DispatchQueue.main.async { self?.polling = false; self?.handlePlayer(data, resp as? HTTPURLResponse) }
        }.resume()
    }

    private func handlePlayer(_ data: Data?, _ resp: HTTPURLResponse?) {
        switch resp?.statusCode ?? 0 {
        case 200: break
        case 204: setSong(nil, note: "Nothing playing"); return
        case 401: accessToken = nil; return
        case 429:
            let wait = Double(resp?.value(forHTTPHeaderField: "Retry-After") ?? "") ?? 30
            backoffUntil = Date().addingTimeInterval(wait)
            return
        default: return   // network hiccup: keep the last song; the display ages it out
        }
        guard let data, let j = try? JSONSerialization.jsonObject(with: data) as? [String: Any] else { return }
        let device = j["device"] as? [String: Any]
        let type = (device?["type"] as? String ?? "").lowercased()
        // Only what this phone (or a car head unit) is playing, not a speaker at home.
        guard ["smartphone", "automobile", "carthing"].contains(type) else {
            setSong(nil, note: "Playing on \(device?["name"] as? String ?? "another device")"); return
        }
        guard j["is_playing"] as? Bool == true, let item = j["item"] as? [String: Any],
              let title = item["name"] as? String else {
            setSong(nil, note: "Paused"); return
        }
        var artist = ((item["artists"] as? [[String: Any]]) ?? []).compactMap { $0["name"] as? String }.joined(separator: ", ")
        if artist.isEmpty, let show = item["show"] as? [String: Any] { artist = show["name"] as? String ?? "" }
        setSong(Song(title: title, artist: artist), note: "")
    }

    private func setSong(_ s: Song?, note: String) {
        self.note = note
        if s != song { song = s; onChange?() }
    }

    // MARK: helpers

    private static func randomString(_ n: Int) -> String {
        let chars = Array("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-._~")
        var bytes = [UInt8](repeating: 0, count: n)
        _ = SecRandomCopyBytes(kSecRandomDefault, n, &bytes)
        return String(bytes.map { chars[Int($0) % chars.count] })
    }
}

extension Spotify: ASWebAuthenticationPresentationContextProviding {
    func presentationAnchor(for session: ASWebAuthenticationSession) -> ASPresentationAnchor {
        UIApplication.shared.connectedScenes.compactMap { ($0 as? UIWindowScene)?.keyWindow }.first ?? ASPresentationAnchor()
    }
}

private extension Data {
    var base64URL: String {
        base64EncodedString().replacingOccurrences(of: "+", with: "-")
            .replacingOccurrences(of: "/", with: "_").replacingOccurrences(of: "=", with: "")
    }
}

private extension String {
    var formEncoded: String {
        var allowed = CharacterSet.alphanumerics
        allowed.insert(charactersIn: "-._~")
        return addingPercentEncoding(withAllowedCharacters: allowed) ?? self
    }
}

/// Minimal Keychain storage for the refresh token (this device only).
enum Keychain {
    private static func query(_ key: String) -> [String: Any] {
        [kSecClass as String: kSecClassGenericPassword, kSecAttrService as String: "dev.backseatdriver",
         kSecAttrAccount as String: key]
    }
    static func set(_ key: String, _ value: String) {
        SecItemDelete(query(key) as CFDictionary)
        var q = query(key)
        q[kSecValueData as String] = Data(value.utf8)
        q[kSecAttrAccessible as String] = kSecAttrAccessibleAfterFirstUnlockThisDeviceOnly
        SecItemAdd(q as CFDictionary, nil)
    }
    static func get(_ key: String) -> String? {
        var q = query(key)
        q[kSecReturnData as String] = true
        var out: AnyObject?
        guard SecItemCopyMatching(q as CFDictionary, &out) == errSecSuccess, let d = out as? Data else { return nil }
        return String(data: d, encoding: .utf8)
    }
    static func delete(_ key: String) { SecItemDelete(query(key) as CFDictionary) }
}
