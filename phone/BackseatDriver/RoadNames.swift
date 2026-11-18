import Foundation

/// Turns what reverse geocoding returns into what a five-year-old reads on a
/// road sign, and pulls out a signed direction only when the name carries one
/// ("US-22 W" -> Route 22, West). Everything else gets the compass bearing.
enum RoadNames {
    struct Result: Equatable {
        var name: String
        var signedDirection: String?   // "North", "South", "East", "West"
    }

    private static let dirWords = ["N": "North", "S": "South", "E": "East", "W": "West",
                                   "NB": "North", "SB": "South", "EB": "East", "WB": "West",
                                   "NORTH": "North", "SOUTH": "South", "EAST": "East", "WEST": "West"]

    static func normalize(_ raw: String) -> Result {
        let s = raw.trimmingCharacters(in: .whitespaces)
        // Interstate: "I-78", "I 78", "Interstate 78", optional direction suffix
        if let m = match(s, #"^(?:I|Interstate)[\s-]*(\d{1,3})(?:\s+(N|S|E|W|NB|SB|EB|WB|North|South|East|West))?$"#) {
            return Result(name: "Interstate \(m[1])", signedDirection: direction(m[2]))
        }
        // US / state routes: "US-22", "US Route 22", "U.S. 22", "PA-378", "SR 378", "State Route 378", "Route 22"
        if let m = match(s, #"^(?:U\.?S\.?|US Highway|US Route|PA|Pa\.|SR|State Route|State Highway|Route|Rte\.?|Hwy)[\s-]*(\d{1,4})(?:\s+(N|S|E|W|NB|SB|EB|WB|North|South|East|West))?$"#) {
            return Result(name: "Route \(m[1])", signedDirection: direction(m[2]))
        }
        return Result(name: s, signedDirection: nil)
    }

    /// 8-point compass name for a course in degrees.
    static func bearing(_ course: Double) -> String {
        let names = ["North", "Northeast", "East", "Southeast", "South", "Southwest", "West", "Northwest"]
        let i = Int(((course.truncatingRemainder(dividingBy: 360) + 360 + 22.5) / 45).rounded(.down)) % 8
        return names[i]
    }

    private static func direction(_ token: String?) -> String? {
        guard let t = token, !t.isEmpty else { return nil }
        return dirWords[t.uppercased()]
    }

    private static func match(_ s: String, _ pattern: String) -> [String?]? {
        guard let re = try? NSRegularExpression(pattern: pattern, options: [.caseInsensitive]),
              let m = re.firstMatch(in: s, range: NSRange(s.startIndex..., in: s)) else { return nil }
        return (0..<m.numberOfRanges).map { i in
            Range(m.range(at: i), in: s).map { String(s[$0]) }
        }
    }
}
