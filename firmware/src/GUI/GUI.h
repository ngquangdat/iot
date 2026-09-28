#ifndef __GUI_H
#define __GUI_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    MODE_PICTURE = 0,
    MODE_CALENDAR = 1,
    MODE_CLOCK = 2,
} display_mode_t;

// Maneuver icons (NAV command, byte 2)
typedef enum {
    NAV_STRAIGHT = 0,
    NAV_LEFT,
    NAV_RIGHT,
    NAV_SLIGHT_LEFT,
    NAV_SLIGHT_RIGHT,
    NAV_SHARP_LEFT,
    NAV_SHARP_RIGHT,
    NAV_KEEP_LEFT,
    NAV_KEEP_RIGHT,
    NAV_UTURN_LEFT,
    NAV_UTURN_RIGHT,
    NAV_ROUNDABOUT,
    NAV_MERGE,
    NAV_ARRIVE,
} nav_icon_t;

#define NAV_FLAG_FORCE 0x01    // full refresh now
#define NAV_FLAG_ARRIVED 0x02  // destination reached
#define NAV_FLAG_URGENT 0x04   // maneuver is close: don't spend 13 s on a periodic full refresh now
#define NAV_FLAG_CLEANUP 0x08  // long straight ahead: good moment for a ghost-clearing full refresh
#define NAV_FLAG_SHAPE 0x10    // draw the junction sketch (last NAV_SHAPE packet) instead of the text
#define NAV_TEXT_MAX 160
#define NAV_SHAPE_MAX 128

typedef struct {
    uint8_t flags;
    uint8_t icon;
    uint16_t dist_m;     // distance to the maneuver
    uint16_t remain_10m; // remaining route distance, 10 m units
    uint8_t eta_h, eta_m;
    uint8_t text_len;
    char text[NAV_TEXT_MAX];  // UTF-8 instruction
    // Junction sketch in the right panel (138x96 px, origin 108,4):
    // jx jy mpp10 nlines, then per line: width npts x0 y0 x1 y1 ... The last line is the route.
    uint8_t shape_len;
    uint8_t shape[NAV_SHAPE_MAX];
    uint16_t scroll_px;  // set by the firmware: marquee offset of the one-line text above the sketch
} nav_data_t;

#define NAV_MARQUEE_GAP 32  // blank pixels between the end of the text and its repeat

#define GUI_W 250
#define GUI_H 128
#define GUI_BUF_SIZE (GUI_W * GUI_H / 8)

// Frame buffer in panel RAM order (column-first from the right edge), 1 = white.
uint8_t* gui_buffer(void);

void gui_draw_nav(const nav_data_t* nav);
// Width the one-line instruction needs above the sketch (0 if it fits without scrolling).
int gui_nav_marquee_width(const nav_data_t* nav);
void gui_draw_clock(uint32_t local_ts, uint16_t mv, int8_t temp);
void gui_draw_date(uint32_t local_ts, uint16_t mv, int8_t temp);

#endif
