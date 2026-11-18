/*
 * Fonts generated with lv_font_conv 1.5.3 (--no-compress, 4 bpp, ASCII 0x20-0x7E) from the
 * TTFs in ttf/. All are SIL Open Font License (Google Fonts / Fontsource).
 *
 *   bold  = Overpass Bold      stands in for "Interstate Bold" in the Figma
 *   semi  = Overpass SemiBold  stands in for "Navigo Medium"
 *   label = ABeeZee Regular    same as the Figma
 *
 * Interstate and Navigo are commercial fonts; swap them in only with a
 * license that permits embedding in a device. Regenerate, e.g.:
 *   lv_font_conv --font ttf/overpass-700.ttf -r 0x20-0x7E --size 40 --bpp 4 --no-compress \
 *     --format lvgl --lv-include lvgl.h -o bsd_font_bold_40.c
 */
#ifndef BSD_FONTS_H
#define BSD_FONTS_H

#include "lvgl.h"

LV_FONT_DECLARE(bsd_font_bold_40)
LV_FONT_DECLARE(bsd_font_bold_35)
LV_FONT_DECLARE(bsd_font_bold_32)
LV_FONT_DECLARE(bsd_font_bold_28)
LV_FONT_DECLARE(bsd_font_bold_25)
LV_FONT_DECLARE(bsd_font_bold_20)
LV_FONT_DECLARE(bsd_font_semi_25)
LV_FONT_DECLARE(bsd_font_semi_24)
LV_FONT_DECLARE(bsd_font_semi_20)
LV_FONT_DECLARE(bsd_font_label_20)
LV_FONT_DECLARE(bsd_font_label_18)

#endif
