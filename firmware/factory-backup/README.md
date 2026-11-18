# Factory flash backup

> The `.bin` image itself is not in this repository (it's Waveshare's firmware). Make your own before flashing:
> `uvx esptool --port /dev/cu.usbmodemXXXX --baud 230400 read-flash 0 0x1000000 factory-flash-16MB.bin`

A full image of the board's 16 MB flash as it arrived from Waveshare, read on November 16, 2026, before any of our firmware was written to it.

| | |
|---|---|
| File | `factory-flash-16MB.bin` (16,777,216 bytes, offset 0 to 0x1000000) |
| SHA-256 | `65f192852960e838dc450f2d781d2b50b173db94704d1320baa4bd19e202b8c1` |
| Verified | `esptool verify-flash` against the board: digest matched |
| Chip | ESP32-S3 (QFN56) rev v0.2, 8 MB embedded PSRAM, 16 MB quad flash (ID 46/4018) |
| Tool | esptool v5.6.0 |

Partition table at 0x8000:

| Name | Type | Offset | Size |
|---|---|---|---|
| nvs | data | 0x009000 | 24 KB |
| phy_init | data | 0x00f000 | 4 KB |
| factory | app | 0x010000 | 5120 KB |

Only the first 1.94 MB holds data; the rest is erased (0xFF).

## Restore

Plug into the **UART** USB-C port. Faster baud rates (460800 and 921600) failed with serial corruption on this cable and Mac, so use 230400.

```bash
uvx esptool --port /dev/cu.usbmodemXXXX --baud 230400 write-flash 0 factory-flash-16MB.bin
```

Then confirm:

```bash
uvx esptool --port /dev/cu.usbmodemXXXX --baud 230400 verify-flash 0 factory-flash-16MB.bin
```

The port name can change if the board is plugged into a different USB port; check with `ls /dev/cu.usbmodem*`.
