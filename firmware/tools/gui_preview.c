// Host-side preview of the on-device screens: renders to PBM files.
// cc -Isrc/GUI tools/gui_preview.c src/GUI/GUI.c src/GUI/font_data.c -o build/gui_preview
#include <stdio.h>
#include <string.h>

#include "GUI.h"

static void dump(const char* path) {
    const uint8_t* fb = gui_buffer();
    FILE* f = fopen(path, "w");
    fprintf(f, "P1\n%d %d\n", GUI_W, GUI_H);
    for (int y = 0; y < GUI_H; y++) {
        for (int x = 0; x < GUI_W; x++) {
            int bit = fb[(GUI_W - 1 - x) * (GUI_H / 8) + (y >> 3)] & (0x80 >> (y & 7));
            fputs(bit ? "0 " : "1 ", f);
        }
        fputc('\n', f);
    }
    fclose(f);
}

static const uint8_t sketch_left[] = {
    // Bytes the web sends for a right turn at a 4-way junction (from the OSRM test route).
    69, 48, 21, 3, 3, 2, 69, 48, 67, 3, 3, 2, 69, 48, 24, 50, 9, 3, 70, 76, 69, 48, 102, 47,
};

static uint8_t black_plane[GUI_BUF_SIZE];

// Render a two-pass (black + red) screen to a colour PPM.
static void dump_color(const char* path, void (*draw)(uint32_t, uint16_t, int8_t), uint32_t ts, uint16_t mv, int8_t t) {
    gui_set_red_pass(false);
    draw(ts, mv, t);
    memcpy(black_plane, gui_buffer(), GUI_BUF_SIZE);
    gui_set_red_pass(true);
    draw(ts, mv, t);
    gui_set_red_pass(false);
    const uint8_t* red = gui_buffer();
    FILE* f = fopen(path, "w");
    fprintf(f, "P3\n%d %d\n255\n", GUI_W, GUI_H);
    for (int y = 0; y < GUI_H; y++)
        for (int x = 0; x < GUI_W; x++) {
            int i = (GUI_W - 1 - x) * (GUI_H / 8) + (y >> 3), m = 0x80 >> (y & 7);
            if (!(red[i] & m)) fputs("220 0 0 ", f);
            else if (!(black_plane[i] & m)) fputs("0 0 0 ", f);
            else fputs("255 255 255 ", f);
        }
    fclose(f);
}

static void alert_a(uint32_t ts, uint16_t mv, int8_t t) {
    const char* s = "Họp giao ban với phòng Phân tích, mang báo cáo quý III";
    gui_draw_alert(ts, s, strlen(s), 0);
}
static void alert_b(uint32_t ts, uint16_t mv, int8_t t) {
    const char* s = "Uống thuốc";
    gui_draw_alert(ts, s, strlen(s), 2);
}

static uint16_t nav_scroll;

static void nav(const char* path, uint8_t icon, uint16_t dist, const char* text, uint8_t flags) {
    nav_data_t n = {flags, icon, dist, 420, 14, 35, 0, {0}, 0, {0}};
    if (flags & NAV_FLAG_SHAPE) {
        n.shape_len = sizeof(sketch_left);
        memcpy(n.shape, sketch_left, sizeof(sketch_left));
    }
    n.text_len = strlen(text);
    n.scroll_px = nav_scroll;
    memcpy(n.text, text, n.text_len);
    gui_draw_nav(&n);
    dump(path);
}

int main(void) {
    nav("build/nav_right.pbm", NAV_RIGHT, 250, "Rẽ phải vào Lê Thánh Tôn", 0);
    nav("build/nav_long.pbm", NAV_ROUNDABOUT, 1250,
        "Vào vòng xuyến, ra lối thứ 2 vào Đại lộ Nguyễn Văn Linh rồi đi tiếp thẳng về phía Quận 7", 0);
    nav("build/nav_arrive.pbm", NAV_ARRIVE, 0, "", NAV_FLAG_ARRIVED);
    nav("build/nav_400.pbm", NAV_LEFT, 400, "Rẽ trái vào Nguyễn Trãi", 0);
    nav("build/nav_150.pbm", NAV_LEFT, 150, "Rẽ trái vào Nguyễn Trãi", 0);
    nav("build/nav_40.pbm", NAV_LEFT, 40, "Rẽ trái vào Nguyễn Trãi", 0);
    nav("build/nav_10.pbm", NAV_LEFT, 10, "Rẽ trái vào Nguyễn Trãi", 0);
    nav("build/sketch_120.pbm", NAV_RIGHT, 120, "Rẽ phải vào Đại lộ Thăng Long", NAV_FLAG_SHAPE);
    nav_scroll = 112;
    nav("build/sketch_30.pbm", NAV_RIGHT, 40, "Rẽ phải vào Đại lộ Thăng Long", NAV_FLAG_SHAPE);
    for (int i = 0; i <= NAV_ARRIVE; i++) {
        char p[40];
        snprintf(p, sizeof p, "build/icon_%02d.pbm", i);
        nav(p, i, 80, "Đi thẳng", 0);
    }
    // 2026-10-10 21:13 local (UTC+7), Saturday, lunar 1/9 Bính Ngọ
    dump_color("build/clock_a.ppm", gui_draw_clock, 1791641580 + 7 * 3600, 2950, 27);
    // 2025-07-30 08:05: lunar 6 nhuận (leap month), month starting on Tuesday, 6 calendar rows check
    dump_color("build/clock_b.ppm", gui_draw_clock, 1753837500 + 7 * 3600, 2700, 31);
    // 2026-02-17 00:00 Tết Bính Ngọ
    dump_color("build/clock_c.ppm", gui_draw_clock, 1771261200 + 7 * 3600, 3000, 18);
    // 2026-08-31: a 6-row month (August 2026 starts on Saturday)
    dump_color("build/clock_d.ppm", gui_draw_clock, 1788152400 + 7 * 3600, 2400, 33);
    gui_draw_clock(1790000000 + 7 * 3600, 2950, 27);
    dump("build/clock.pbm");
    const char* rem = "Họp giao ban với phòng Phân tích, mang báo cáo quý III";
    gui_set_clock_banner(1791640800 + 7 * 3600 - 3600, rem, strlen(rem));
    dump_color("build/banner_a.ppm", gui_draw_clock, 1791641580 + 7 * 3600, 2950, 27);   // Oct 2026: 5 rows
    gui_set_clock_banner(1788152400 + 7 * 3600 - 7200, "Uống thuốc", strlen("Uống thuốc"));
    dump_color("build/banner_b.ppm", gui_draw_clock, 1788152400 + 7 * 3600, 2400, 33);   // Aug 2026: 6 rows
    gui_set_clock_banner(0, "", 0);
    dump_color("build/alert_a.ppm", alert_a, 1791641580 + 7 * 3600, 0, 0);
    dump_color("build/alert_b.ppm", alert_b, 1791641580 + 7 * 3600, 0, 0);
    gui_draw_date(1790000000 + 7 * 3600, 2950, 27);
    dump("build/date.pbm");
    gui_draw_clock(1790000000 + 7 * 3600, 2600, 24);
    dump("build/clock_low.pbm");
    return 0;
}
