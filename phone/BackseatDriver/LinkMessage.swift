import Foundation

/// The phone-to-display message format. Mirrors app/include/bsd_link.h.
enum LinkMessage {
    static let serviceUUID = "AFF99621-C1CF-42E7-B93A-86561359C9B6"
    static let feedUUID    = "472B9109-1271-40ED-A3B9-CD569D58DBA6"
    static let sep = "\u{1F}"

    enum DirectionKind: String { case none = "N", signed = "S", bearing = "B" }

    static func road(name: String, kind: DirectionKind, direction: String) -> Data {
        encode(["R", kind.rawValue, ascii(direction), ascii(name, limit: 95)])
    }
    static let noRoad = Data("r".utf8)

    static func song(title: String, artist: String, playing: Bool) -> Data {
        encode(["S", playing ? "1" : "0", ascii(title, limit: 127), ascii(artist, limit: 95)])
    }
    static let noSong = Data("s".utf8)

    static func light(day: Bool) -> Data { encode(["L", day ? "d" : "n"]) }

    private static func encode(_ fields: [String]) -> Data {
        Data(fields.joined(separator: sep).utf8)
    }

    /// The display's fonts cover printable ASCII only: transliterate, strip
    /// accents, map typographic punctuation, drop anything else.
    static func ascii(_ s: String, limit: Int = 60) -> String {
        var t = s
        for (from, to) in [("\u{2018}", "'"), ("\u{2019}", "'"), ("\u{201C}", "\""), ("\u{201D}", "\""),
                           ("\u{2013}", "-"), ("\u{2014}", "-"), ("\u{2026}", "..."), ("\u{00A0}", " ")] {
            t = t.replacingOccurrences(of: from, with: to)
        }
        t = t.applyingTransform(.toLatin, reverse: false) ?? t
        t = t.applyingTransform(.stripDiacritics, reverse: false) ?? t
        let kept = t.unicodeScalars.filter { $0.value >= 0x20 && $0.value <= 0x7E }
        var out = String(String.UnicodeScalarView(kept))
        while out.contains("  ") { out = out.replacingOccurrences(of: "  ", with: " ") }
        out = out.trimmingCharacters(in: .whitespaces)
        return String(out.prefix(limit))
    }
}
