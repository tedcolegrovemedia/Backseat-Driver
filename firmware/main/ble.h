/*
 * ble.h — NimBLE peripheral that receives bsd_link messages from the phone.
 *
 * BLE callbacks run on the NimBLE host task. They never touch bsd_feed; they
 * copy each write into a queue that the UI task drains with ble_poll().
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

void ble_start(void);

/* UI task: apply queued phone messages and link changes. Returns true if any
 * phone message arrived since the last call. */
bool ble_poll(uint32_t now_ms);
