/*
 * bsd_link.h — the phone-to-display message format (BLE GATT writes).
 *
 * One GATT write = one message. A message is ASCII text: a one-letter type,
 * then fields separated by 0x1F (ASCII "unit separator", so titles can hold
 * any printable character). The phone strips accents and anything outside
 * 0x20-0x7E first, because the display fonts are ASCII only.
 *
 *   R <US> kind <US> direction <US> road name   kind: N none, S signed, B bearing
 *   r                                           road unknown
 *   S <US> playing <US> title <US> artist       playing: 1 or 0
 *   s                                           no song
 *   L <US> d|n                                  day or night look
 *
 * The phone repeats R and S every few seconds while they are valid, so the
 * display's freshness timers (dim at 15 s, "finding the road" at 90 s) only
 * fire when the phone really stops sending.
 *
 * Service      AFF99621-C1CF-42E7-B93A-86561359C9B6
 * Characteristic 472B9109-1271-40ED-A3B9-CD569D58DBA6 (write, write without response)
 */
#ifndef BSD_LINK_H
#define BSD_LINK_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BSD_LINK_SERVICE_UUID "AFF99621-C1CF-42E7-B93A-86561359C9B6"
#define BSD_LINK_FEED_UUID    "472B9109-1271-40ED-A3B9-CD569D58DBA6"
#define BSD_LINK_MSG_MAX      400
#define BSD_LINK_SEP          '\x1f'

/* Applies one message to the feed. Returns false (and changes nothing) if the
 * message is malformed. Must run on the LVGL task, like every bsd_feed call. */
bool bsd_link_apply(const char *msg, size_t len, uint32_t now_ms);

#ifdef __cplusplus
}
#endif
#endif
