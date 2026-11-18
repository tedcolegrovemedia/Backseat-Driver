# Board firmware (ESP32-S3-Touch-LCD-4.3)

Running on the board since November 16: it boots into the child screen with the demo, and the display and touch work.
The factory flash is backed up in `factory-backup/`.

## Build and flash

```bash
firmware/build.sh          # build
firmware/build.sh flash    # build + flash (about 25 s at 230400 baud)
firmware/build.sh monitor  # serial log
```

`build.sh` mirrors this folder to `~/.cache/backseat-fw` (with `app/` linked from the share) and builds there,
so ESP-IDF v5.5.6 (`~/esp/esp-idf-v5.5.6`) never writes to the network share.
`main/board.c` is the board glue: RGB panel, GT911 touch and the CH422G expander, driven directly with no esp_lvgl_port.

## Board facts (from Waveshare's "What's On Board" diagram)

| # | Part | What it means for us |
|---|---|---|
| 1 | **ESP32-S3N16R8** module | 16 MB flash and 8 MB **octal** PSRAM, so set `CONFIG_SPIRAM_MODE_OCT=y` and `CONFIG_ESPTOOLPY_FLASHSIZE_16MB=y`. The retailer's "0.5 MB RAM / 8 MB storage" is the S3's internal 512 KB SRAM, with the PSRAM size listed as storage. The manufacturer's figures are the ones to trust. |
| 5 | USB-C **UART** port (CH343P, #21) | Flash and serial logs from this port. It works with any normal USB-C cable and needs no driver mode switching. |
| 6 | USB-C **USB** port (native S3 USB) | Shares its pins with CAN through the FSUSB42UMX switch (#3), which CH422G EXIO5 selects. Use this port for car power if it's the more convenient side, but flash through #5. |
| 9 / 10 | BOOT / RESET buttons | If auto-download fails: hold BOOT, tap RESET, release BOOT. |
| 11 | MP3302 backlight boost | The backlight is switched by CH422G EXIO2. **Check Monday whether brightness can be dimmed or only switched on and off.** If it can't be dimmed, the dark night theme is our only glare control. |
| 13 | microSD slot | Not needed for the demo, since the catalog and fonts are compiled in. Later it could hold a bigger catalog or a saved garage. |
| 14 | "Sensor AD" header | Optional later: a light sensor here could pick day or night on the board itself. Today the phone decides. |
| 18 / 23 | 3.7 V Li-ion PH2.0 header + CS8501 charger | Optional: a small battery would keep the board running when the car restarts, so the garage and current car survive. Today the garage is cleared on power loss. |
| 24 | PWR / CHG / DONE LEDs | The PWR LED may be visible through the enclosure at night. Cover it if so. |

## Stack

| Piece | Choice | Why |
|---|---|---|
| ESP-IDF | **v5.5.x** | Waveshare's FAQ recommends v5.5.2. Their demos target 5.3 to 5.5. IDF 6.x exists, but none of the board's examples have been tested on it. |
| LVGL | **9.5.0** | Matches the simulator. Waveshare's own demo uses **LVGL 8.3**, so treat their code as a pin and timing reference, not as a base. |
| LVGL glue | own `main/board.c` | Smaller than esp_lvgl_port and has no extra version to match |
| Display | RGB parallel 800×480 via `esp_lcd_panel_rgb` | Waveshare's FAQ lists ST7701. The docs page names no IC. |
| Touch | GT911 over I2C (SDA GPIO8, SCL GPIO9, IRQ GPIO4) | Waveshare wiki FAQ |
| IO expander | CH422G: EXIO1 touch reset, EXIO2 backlight, EXIO4 SD CS, EXIO5 USB/CAN select | Waveshare docs and the board diagram |

The full RGB pin map is on https://docs.waveshare.com/ESP32-S3-Touch-LCD-4.3. Copy it from Waveshare's
demo `ESP_Panel_Board_Custom.h` / `08_lvgl_Porting` instead of retyping it.

## Monday checklist

**Done on November 16:** steps 1 to 7. Step 8 is partly done: the board has 160 KB internal and 6.7 MB PSRAM free after the first frame, and touch works. ESP32-S3 rev v0.2, 8 MB PSRAM, 16 MB flash.
The board shipped with Waveshare's LVGL Widgets demo. Rewriting it from the backup worked, and the display and touch both work.
Serial reads fail above 230400 baud on this cable and Mac. Compressed writes at 230400 take about a minute for a full image.

1. Read the revision off the silkscreen ("ESP32-S3-LCD-4.3", SKU 25948 for touch; the 4.3**B** is a different board). Confirm 8 MB PSRAM and 16 MB flash at boot with `esp_psram_get_size` and `esp_flash_get_size`.
2. Plug into the **UART** USB-C port and flash Waveshare's own LVGL demo unmodified, to prove the display and touch work.
3. Create an IDF project here whose `main` component compiles `../../app/src/*.c` and `../../app/fonts/*.c`, the same sources the simulator uses.
4. Port the `sim/lv_conf.h` options to sdkconfig: RGB565, Montserrat 16 (status line and Bluetooth icon), `LV_USE_STDLIB_MALLOC=CLIB` so buffers land in PSRAM, and snapshot off.
5. Call every `bsd_feed_set_*` and `bsd_ui_*` on the LVGL task (or under `lvgl_port_lock`). BLE callbacks run on another task and must hand their updates over.
6. Frame buffers go in PSRAM. The two 32 KB background tiles are heap-allocated and should land there too; confirm with the memory numbers in step 8. Use bounce buffers if the RGB panel drifts or tears while flash or Wi-Fi is busy (a known ESP32-S3 RGB issue).
7. Boot straight into `bsd_ui_create()` with the demo source running and no menus.
8. Measure:
   - free internal SRAM and PSRAM after boot
   - redraw smoothness of the text fade over the textured background
   - touch accuracy through the enclosure, including FOUND IT! (155×76) and SKIP (155×48) with his fingers
   - backlight dimming (see #11 above)
   - current draw from a car USB port
