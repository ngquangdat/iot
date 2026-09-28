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

static void nav(const char* path, uint8_t icon, uint16_t dist, const char* text, uint8_t flags) {
    nav_data_t n = {flags, icon, dist, 420, 14, 35, 0, {0}};
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
