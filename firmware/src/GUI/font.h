#ifndef __FONT_H
#define __FONT_H

#include <stdint.h>

typedef struct {
    uint16_t cp;     // Unicode code point
    uint8_t w, h;    // bitmap size
    int8_t xo, yo;   // bitmap offset from the pen position on the baseline
    uint8_t adv;     // horizontal advance
    uint16_t off;    // offset into the bitmap array
} font_glyph_t;

typedef struct {
    const font_glyph_t* glyphs;  // sorted by code point
    uint16_t count;
    const uint8_t* bitmap;
    uint8_t ascent;
    uint8_t line_height;
} font_t;

extern const font_t font_text;   // 15 px bold, Vietnamese
extern const font_t font_small;  // 12 px, Vietnamese
extern const font_t font_big;    // 28 px bold digits
extern const font_t font_huge;   // 54 px bold digits

#endif
