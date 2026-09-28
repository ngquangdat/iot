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
    69, 60, 17, 3, 3, 2, 69, 60, 67, 15, 3, 2, 69, 60, 24, 62, 9, 3, 70, 94, 69, 60, 109, 58,
};

static void nav(const char* path, uint8_t icon, uint16_t dist, const char* text, uint8_t flags) {
    nav_data_t n = {flags, icon, dist, 420, 14, 35, 0, {0}, 0, {0}};
    if (flags & NAV_FLAG_SHAPE) {
        n.shape_len = sizeof(sketch_left);
        memcpy(n.shape, sketch_left, sizeof(sketch_left));
    }
    n.text_len = strlen(text);
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
    nav("build/sketch_120.pbm", NAV_RIGHT, 120, "Rẽ phải", NAV_FLAG_SHAPE);
    nav("build/sketch_30.pbm", NAV_RIGHT, 40, "Rẽ phải", NAV_FLAG_SHAPE);
    for (int i = 0; i <= NAV_ARRIVE; i++) {
        char p[40];
        snprintf(p, sizeof p, "build/icon_%02d.pbm", i);
        nav(p, i, 80, "Đi thẳng", 0);
    }
    gui_draw_clock(1790000000 + 7 * 3600, 2950, 27);
    dump("build/clock.pbm");
    gui_draw_date(1790000000 + 7 * 3600, 2950, 27);
    dump("build/date.pbm");
    return 0;
}
