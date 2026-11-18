import CoreBluetooth
import Foundation

/// Finds the headrest display, stays connected, and reconnects on its own
/// after the car (and the board) power-cycles. Writes are queued and sent one
/// at a time with response, so long messages and ordering are both safe.
final class BLELink: NSObject, ObservableObject {
    enum State: String { case off = "Bluetooth off", searching = "Searching", connecting = "Connecting", connected = "Connected" }

    @Published private(set) var state: State = .off
    var onReady: (() -> Void)?

    private let serviceID = CBUUID(string: LinkMessage.serviceUUID)
    private let feedID = CBUUID(string: LinkMessage.feedUUID)
    private let savedKey = "bsd.displayID"
    private var central: CBCentralManager!
    private var display: CBPeripheral?
    private var feed: CBCharacteristic?
    private var queue: [Data] = []
    private var writing = false

    override init() {
        super.init()
        central = CBCentralManager(delegate: self, queue: .main,
                                   options: [CBCentralManagerOptionRestoreIdentifierKey: "bsd.central"])
    }

    func send(_ data: Data) {
        guard feed != nil else { return }
        // Keep the queue short: newer state replaces stale backlog.
        if queue.count > 8 { queue.removeFirst(queue.count - 8) }
        queue.append(data)
        pump()
    }

    private func pump() {
        guard !writing, let p = display, let c = feed, !queue.isEmpty else { return }
        writing = true
        p.writeValue(queue.removeFirst(), for: c, type: .withResponse)
    }

    private func connect(_ p: CBPeripheral) {
        display = p
        p.delegate = self
        state = .connecting
        central.connect(p, options: nil)   // pending forever; iOS completes it when the board appears
    }

    private func findDisplay() {
        if let s = UserDefaults.standard.string(forKey: savedKey), let id = UUID(uuidString: s),
           let p = central.retrievePeripherals(withIdentifiers: [id]).first {
            connect(p)   // also scan, in case the saved one was a different board
        }
        if let p = central.retrieveConnectedPeripherals(withServices: [serviceID]).first {
            connect(p); return
        }
        state = display == nil ? .searching : state
        central.scanForPeripherals(withServices: [serviceID], options: nil)
    }
}

extension BLELink: CBCentralManagerDelegate {
    func centralManagerDidUpdateState(_ c: CBCentralManager) {
        if c.state == .poweredOn {
            if let p = display, p.state == .connected { p.discoverServices([serviceID]) } else { findDisplay() }
        } else {
            state = .off
            feed = nil
        }
    }

    func centralManager(_ c: CBCentralManager, willRestoreState dict: [String: Any]) {
        if let p = (dict[CBCentralManagerRestoredStatePeripheralsKey] as? [CBPeripheral])?.first {
            display = p
            p.delegate = self
        }
    }

    func centralManager(_ c: CBCentralManager, didDiscover p: CBPeripheral,
                        advertisementData: [String: Any], rssi: NSNumber) {
        c.stopScan()
        connect(p)
    }

    func centralManager(_ c: CBCentralManager, didConnect p: CBPeripheral) {
        c.stopScan()
        UserDefaults.standard.set(p.identifier.uuidString, forKey: savedKey)
        p.discoverServices([serviceID])
    }

    func centralManager(_ c: CBCentralManager, didFailToConnect p: CBPeripheral, error: Error?) {
        connect(p)
    }

    func centralManager(_ c: CBCentralManager, didDisconnectPeripheral p: CBPeripheral, error: Error?) {
        feed = nil
        queue.removeAll()
        writing = false
        connect(p)
        state = .searching
    }
}

extension BLELink: CBPeripheralDelegate {
    func peripheral(_ p: CBPeripheral, didDiscoverServices error: Error?) {
        guard let s = p.services?.first(where: { $0.uuid == serviceID }) else { return }
        p.discoverCharacteristics([feedID], for: s)
    }

    func peripheral(_ p: CBPeripheral, didDiscoverCharacteristicsFor s: CBService, error: Error?) {
        guard let c = s.characteristics?.first(where: { $0.uuid == feedID }) else { return }
        feed = c
        state = .connected
        onReady?()
    }

    func peripheral(_ p: CBPeripheral, didWriteValueFor c: CBCharacteristic, error: Error?) {
        writing = false
        pump()
    }
}
