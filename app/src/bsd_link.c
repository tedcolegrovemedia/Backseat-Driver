#include "bsd_link.h"
#include "bsd_feed.h"

#include <string.h>

#define MAX_FIELDS 4

/* Splits a copy of msg on 0x1F. Non-printable bytes become spaces so nothing
 * odd reaches the fonts. Returns the field count. */
static int split(const char *msg, size_t len, char *buf, char *field[MAX_FIELDS])
{
    if (len >= BSD_LINK_MSG_MAX) len = BSD_LINK_MSG_MAX - 1;
    int n = 0;
    field[n++] = buf;
    for (size_t i = 0; i < len; i++) {
        char c = msg[i];
        if (c == BSD_LINK_SEP && n < MAX_FIELDS) { buf[i] = '\0'; field[n++] = &buf[i + 1]; continue; }
        buf[i] = (c < 0x20 || c > 0x7e) ? ' ' : c;
    }
    buf[len] = '\0';
    return n;
}

bool bsd_link_apply(const char *msg, size_t len, uint32_t now_ms)
{
    if (!msg || len == 0) return false;
    char buf[BSD_LINK_MSG_MAX];
    char *f[MAX_FIELDS];
    int n = split(msg, len, buf, f);
    if (strlen(f[0]) != 1) return false;

    switch (f[0][0]) {
    case 'R': {
        if (n != 4) return false;
        bsd_dir_kind_t kind = f[1][0] == 'S' ? BSD_DIR_SIGNED
                            : f[1][0] == 'B' ? BSD_DIR_BEARING : BSD_DIR_NONE;
        if (!f[3][0]) { bsd_feed_clear_road(); return true; }
        bsd_feed_set_road(f[3], kind, f[2], now_ms);
        return true;
    }
    case 'r':
        bsd_feed_clear_road();
        return true;
    case 'S':
        if (n != 4) return false;
        if (!f[2][0]) { bsd_feed_clear_song(); return true; }
        bsd_feed_set_song(f[2], f[3], f[1][0] == '1', now_ms);
        return true;
    case 's':
        bsd_feed_clear_song();
        return true;
    case 'L':
        if (n != 2) return false;
        if (f[1][0] == 'd') bsd_feed_set_light(BSD_LIGHT_DAY);
        else if (f[1][0] == 'n') bsd_feed_set_light(BSD_LIGHT_NIGHT);
        else return false;
        return true;
    default:
        return false;
    }
}
