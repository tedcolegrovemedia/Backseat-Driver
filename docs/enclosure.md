# Enclosure and physical sizes

From Waveshare's "Outline Dimensions" drawing for the ESP32-S3-LCD-4.3. All units are mm. Measure the real board
before cutting anything.

## Display (front)

| | Width | Height |
|---|---|---|
| Glass / panel outline | 105.42 ±0.30 | 67.07 ±0.30 |
| Viewing area | 97.22 ±0.30 | 56.40 ±0.30 |
| **Active area (800×480 px)** | **95.04 ±0.30** | **53.86 ±0.30** |

- Side borders: 5.19 from the active area to the glass edge on each side.
- Top border: 4.20 from the glass edge to the viewing area. The drawing shows the vertical borders as unequal, so check the bottom against the real panel.
- The bezel opening should match the **viewing area** (97.22 × 56.40), with a small overlap so the bezel hides the panel's edge.

## Board (back)

| | |
|---|---|
| PCB outline | **106.00 × 68.00**, slightly larger than the glass |
| Mounting holes | 4 holes, **98.00 × 60.00** centers, 4.00 in from each edge |
| Thickness | about 6.6 at the panel. Allow 4.0 or more behind the PCB for the module, connectors and cables. |
| USB-C ports | Both on the **left edge**: UART about 12.9 from the top, USB about 32.9. Cables exit sideways, so leave room for a right-angle plug. |
| Right-edge headers | Sensor, CAN, I2C, RS485 and battery JST connectors. These aren't needed for the demo, but the enclosure must clear them. |
| BOOT / RESET | Top edge, about 44 and 52 from the left. A pin hole for RESET is handy. |
| microSD | Top-right corner, opening upward |

## Touch targets on the real glass

Pixel pitch is 95.04 / 800 = **0.119 mm per px** horizontally and 53.86 / 480 = **0.112 mm per px** vertically.

| Element | Pixels | On the glass | Note |
|---|---|---|---|
| FOUND IT! | 155 × 76 | **18.4 × 8.5 mm** | Taller than the Figma's 155 × 47 (5.3 mm), close to the guidance below |
| SKIP | 155 × 48 | 18.4 × 5.4 mm | About the Figma size. The row has no room for two 9 mm targets. |
| GARAGE / BACK | 155 × 34 | 18.4 × 3.8 mm | Very small, though that suits a button for grown-ups |
| Gap FOUND IT! → SKIP | 8 px | 0.9 mm | Keep an eye on mis-taps Monday |
| Touch slop added in code | 4 px | 0.45 mm | Already in place |

Common touch guidance is about 7–9 mm minimum per target; Apple's 44 pt is about 7 mm. The Figma's 47 px
buttons were 5.3 mm tall, so on November 15 FOUND IT! became 76 px (8.5 mm) and SKIP stayed at 48 px. The
car row is only 137 px (15.4 mm) inside, too short for two 9 mm targets stacked.

If SKIP proves too small, the next step is side by side: FOUND IT! about 155 × 110 with SKIP about 100 × 110
to its left. Both would be over 12 mm tall, but the car-name column would narrow to about 355 px.

## Road name size on the glass

| Text | Cap height | From a child seat (~60–80 cm) |
|---|---|---|
| Road name (Overpass Bold 40) | about 3.3 mm | Readable but not large. The 32 and 28 px fallbacks for long names are 2.7 and 2.3 mm. |
| Direction (25 px) | about 2.0 mm | Small; check from his seat |
| Car make (35 px) | about 2.9 mm | |

These are estimates from the font's cap height (about 0.7 em) times 0.112 mm per px.
