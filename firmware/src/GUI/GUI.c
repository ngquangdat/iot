#include "GUI.h"

#include <string.h>

#include "font.h"

// Landscape 250x128 canvas over the panel's native 128x250 RAM layout.
// Content stays inside rows 2..121: the glass only shows 122 of the 128 sources.

static uint8_t fb[GUI_BUF_SIZE];

uint8_t* gui_buffer(void) { return fb; }

typedef struct {
    int16_t x, y;
} pt_t;

/* ---------- raster primitives (black on white) ---------- */

static void px(int x, int y) {
    if ((unsigned)x >= GUI_W || (unsigned)y >= GUI_H) return;
    fb[(GUI_W - 1 - x) * (GUI_H / 8) + (y >> 3)] &= ~(0x80 >> (y & 7));
}

static void hline(int x0, int x1, int y) {
    if (x0 > x1) {
        int t = x0;
        x0 = x1;
        x1 = t;
    }
    for (int x = x0; x <= x1; x++) px(x, y);
}

static void fill_rect(int x, int y, int w, int h) {
    for (int j = y; j < y + h; j++) hline(x, x + w - 1, j);
}

static uint32_t isqrt(uint32_t n) {
    uint32_t r = 0, bit = 1UL << 30;
    while (bit > n) bit >>= 2;
    while (bit) {
        if (n >= r + bit) {
            n -= r + bit;
            r = (r >> 1) + bit;
        } else {
            r >>= 1;
        }
        bit >>= 2;
    }
    return r;
}

static void fill_tri(pt_t a, pt_t b, pt_t c) {
    pt_t t;
    if (a.y > b.y) t = a, a = b, b = t;
    if (b.y > c.y) t = b, b = c, c = t;
    if (a.y > b.y) t = a, a = b, b = t;
    if (a.y == c.y) {
        int lo = a.x, hi = a.x;
        if (b.x < lo) lo = b.x;
        if (b.x > hi) hi = b.x;
        if (c.x < lo) lo = c.x;
        if (c.x > hi) hi = c.x;
        hline(lo, hi, a.y);
        return;
    }
    for (int y = a.y; y <= c.y; y++) {
        int xa = a.x + (int32_t)(c.x - a.x) * (y - a.y) / (c.y - a.y);
        int xb;
        if (y < b.y)
            xb = a.x + (int32_t)(b.x - a.x) * (y - a.y) / (b.y - a.y);
        else if (c.y != b.y)
            xb = b.x + (int32_t)(c.x - b.x) * (y - b.y) / (c.y - b.y);
        else
            xb = b.x;
        hline(xa, xb, y);
    }
}

static void fill_circle(int cx, int cy, int r) {
    for (int dy = -r; dy <= r; dy++) {
        int dx = isqrt(r * r - dy * dy);
        hline(cx - dx, cx + dx, cy + dy);
    }
}

// Ring with outer radius r and thickness t.
static void ring(int cx, int cy, int r, int t) {
    int ri = r - t;
    for (int dy = -r; dy <= r; dy++) {
        int dxo = isqrt(r * r - dy * dy);
        if (dy > -ri && dy < ri) {
            int dxi = isqrt(ri * ri - dy * dy);
            hline(cx - dxo, cx - dxi - 1, cy + dy);
            hline(cx + dxi + 1, cx + dxo, cy + dy);
        } else {
            hline(cx - dxo, cx + dxo, cy + dy);
        }
    }
}

static void thick_line(pt_t p0, pt_t p1, int w) {
    int dx = p1.x - p0.x, dy = p1.y - p0.y;
    int len = isqrt(dx * dx + dy * dy);
    if (len == 0) {
        fill_circle(p0.x, p0.y, w / 2);
        return;
    }
    int ox = -dy * w / (2 * len), oy = dx * w / (2 * len);
    pt_t a = {p0.x + ox, p0.y + oy}, b = {p1.x + ox, p1.y + oy};
    pt_t c = {p1.x - ox, p1.y - oy}, d = {p0.x - ox, p0.y - oy};
    fill_tri(a, b, c);
    fill_tri(a, c, d);
}

/* ---------- maneuver icons (ported from the web preview) ---------- */

// Stroke a polyline with round joins and finish it with an arrow head.
static void arrow(const pt_t* pts, int n, int w, int s) {
    int head = s * 3 / 10, half = s / 5;
    pt_t tip = pts[n - 1], prev = pts[n - 2];
    int dx = tip.x - prev.x, dy = tip.y - prev.y;
    int len = isqrt(dx * dx + dy * dy);
    if (len == 0) len = 1;
    pt_t base = {tip.x - dx * head * 9 / (10 * len), tip.y - dy * head * 9 / (10 * len)};
    for (int i = 0; i + 2 < n; i++) {
        thick_line(pts[i], pts[i + 1], w);
        fill_circle(pts[i + 1].x, pts[i + 1].y, w / 2);
    }
    thick_line(pts[n - 2], base, w);
    int px_ = -dy * half / len, py_ = dx * half / len;
    fill_tri(tip, (pt_t){base.x + px_, base.y + py_}, (pt_t){base.x - px_, base.y - py_});
}

// Stem up from the bottom, then a branch at the given angle (sin/cos x1000, 0 = up).
static void bend(int cx, int cy, int s, int sn, int cs, bool centered, pt_t out[3]) {
    bool sharp = cs < -100;
    int bottom = cy + s * 48 / 100;
    int my = sharp ? cy - s * 30 / 100 : cy + s * 5 / 100;
    int L = sharp ? s * 55 / 100 : s * 46 / 100;
    int x0 = centered ? cx : cx - sn * s * 18 / 100000;
    out[0] = (pt_t){x0, bottom};
    out[1] = (pt_t){x0, my};
    out[2] = (pt_t){x0 + sn * L / 1000, my - cs * L / 1000};
}

static void draw_icon(uint8_t icon, int cx, int cy, int s) {
    int w = s * 15 / 100;
    pt_t p[12];
    int bottom = cy + s * 48 / 100, mid = cy + s * 5 / 100;
    switch (icon) {
        case NAV_LEFT:
        case NAV_RIGHT:
        case NAV_SLIGHT_LEFT:
        case NAV_SLIGHT_RIGHT:
        case NAV_SHARP_LEFT:
        case NAV_SHARP_RIGHT: {
            static const int16_t trig[][2] = {{1000, 0}, {707, 707}, {866, -500}};  // 90, 45, 120 degrees
            int k = (icon - NAV_LEFT) / 2, sign = (icon - NAV_LEFT) % 2 ? 1 : -1;
            bend(cx, cy, s, sign * trig[k][0], trig[k][1], false, p);
            arrow(p, 3, w, s);
            break;
        }
        case NAV_KEEP_LEFT:
        case NAV_KEEP_RIGHT: {
            int sign = icon == NAV_KEEP_LEFT ? -1 : 1;
            bend(cx, cy, s, -sign * 574, 819, true, p);  // 35 degrees
            thick_line(p[1], p[2], s * 6 / 100);
            bend(cx, cy, s, sign * 574, 819, true, p);
            arrow(p, 3, w, s);
            break;
        }
        case NAV_UTURN_LEFT:
        case NAV_UTURN_RIGHT: {
            static const int16_t arc[][2] = {{1000, 0}, {866, 500}, {500, 866}, {0, 1000},
                                             {-500, 866}, {-866, 500}, {-1000, 0}};
            int sign = icon == NAV_UTURN_LEFT ? 1 : -1, r = s / 5, top = cy - s * 12 / 100, n = 0;
            p[n++] = (pt_t){cx + sign * r, bottom};
            for (int i = 0; i < 7; i++) p[n++] = (pt_t){cx + sign * arc[i][0] * r / 1000, top - arc[i][1] * r / 1000};
            p[n++] = (pt_t){cx - sign * r, cy + s * 30 / 100};
            arrow(p, n, w, s);
            break;
        }
        case NAV_ROUNDABOUT: {
            int r = s / 5, oy = cy + s * 4 / 100, t = s / 10;
            ring(cx, oy, r + t / 2, t);
            thick_line((pt_t){cx, bottom}, (pt_t){cx, oy + r}, w);
            p[0] = (pt_t){cx, oy - r};
            p[1] = (pt_t){cx, cy - s / 2};
            arrow(p, 2, w, s);
            break;
        }
        case NAV_MERGE:
            thick_line((pt_t){cx - s * 32 / 100, bottom}, (pt_t){cx, mid - s * 5 / 100}, s * 6 / 100);
            p[0] = (pt_t){cx, bottom};
            p[1] = (pt_t){cx, cy - s * 48 / 100};
            arrow(p, 2, w, s);
            break;
        case NAV_ARRIVE: {
            int oy = cy - s / 10, t = s / 10;
            ring(cx, oy, s * 26 / 100 + t / 2, t);
            fill_circle(cx, oy, s / 10);
            fill_tri((pt_t){cx - s * 12 / 100, cy + s * 14 / 100}, (pt_t){cx, cy + s * 46 / 100},
                     (pt_t){cx + s * 12 / 100, cy + s * 14 / 100});
            break;
        }
        default:  // straight
            p[0] = (pt_t){cx, bottom};
            p[1] = (pt_t){cx, cy - s * 48 / 100};
            arrow(p, 2, w, s);
            break;
    }
}

/* ---------- text ---------- */

static uint16_t utf8_next(const char* s, int len, int* i) {
    uint8_t c = (uint8_t)s[(*i)++];
    if (c < 0x80) return c;
    int extra = c >= 0xE0 ? 2 : 1;
    uint16_t cp = c & (extra == 2 ? 0x0F : 0x1F);
    while (extra-- && *i < len) cp = (cp << 6) | ((uint8_t)s[(*i)++] & 0x3F);
    return cp;
}

static const font_glyph_t* glyph(const font_t* f, uint16_t cp) {
    int lo = 0, hi = f->count - 1;
    while (lo <= hi) {
        int m = (lo + hi) / 2;
        if (f->glyphs[m].cp == cp) return &f->glyphs[m];
        if (f->glyphs[m].cp < cp)
            lo = m + 1;
        else
            hi = m - 1;
    }
    return cp == '?' ? NULL : glyph(f, '?');
}

static int text_width(const font_t* f, const char* s, int len) {
    int w = 0, i = 0;
    while (i < len) {
        const font_glyph_t* g = glyph(f, utf8_next(s, len, &i));
        if (g) w += g->adv;
    }
    return w;
}

static int draw_text(const font_t* f, int x, int base, const char* s, int len) {
    int i = 0;
    while (i < len) {
        const font_glyph_t* g = glyph(f, utf8_next(s, len, &i));
        if (!g) continue;
        const uint8_t* bm = f->bitmap + g->off;
        for (int r = 0, k = 0; r < g->h; r++)
            for (int c = 0; c < g->w; c++, k++)
                if (bm[k >> 3] & (0x80 >> (k & 7))) px(x + g->xo + c, base + g->yo + r);
        x += g->adv;
    }
    return x;
}

static void draw_text_center(const font_t* f, int cx, int base, const char* s) {
    int len = strlen(s);
    draw_text(f, cx - text_width(f, s, len) / 2, base, s, len);
}

static void draw_text_right(const font_t* f, int right, int base, const char* s) {
    int len = strlen(s);
    draw_text(f, right - text_width(f, s, len), base, s, len);
}

// Word-wrap into at most max_lines lines, ending with an ellipsis if it doesn't fit.
static void draw_wrapped(const font_t* f, int x, int base, int line_h, int max_w, int max_lines, const char* s,
                         int len) {
    static const char ell[] = "\xE2\x80\xA6";
    int pos = 0;
    for (int line = 0; line < max_lines && pos < len; line++) {
        while (pos < len && s[pos] == ' ') pos++;
        int start = pos, i = pos, brk = -1, w = 0;
        while (i < len) {
            int j = i;
            uint16_t cp = utf8_next(s, len, &j);
            if (cp == ' ') brk = i;
            const font_glyph_t* g = glyph(f, cp);
            int gw = g ? g->adv : 0;
            if (w + gw > max_w) break;
            w += gw;
            i = j;
        }
        int end, next;
        if (i >= len) {
            end = next = len;
        } else if (brk > start) {
            end = brk;
            next = brk + 1;
        } else {
            end = next = i;
        }
        if (line == max_lines - 1 && next < len) {
            int ew = text_width(f, ell, 3);
            end = i;
            while (end > start && text_width(f, s + start, end - start) + ew > max_w) {
                end--;
                while (end > start && ((uint8_t)s[end] & 0xC0) == 0x80) end--;
            }
            int xe = draw_text(f, x, base + line * line_h, s + start, end - start);
            draw_text(f, xe, base + line * line_h, ell, 3);
            return;
        }
        draw_text(f, x, base + line * line_h, s + start, end - start);
        pos = next;
    }
}

/* ---------- formatting ---------- */

static char* put_uint(char* p, uint32_t v, int min_digits) {
    char tmp[10];
    int n = 0;
    do {
        tmp[n++] = '0' + v % 10;
        v /= 10;
    } while (v || n < min_digits);
    while (n) *p++ = tmp[--n];
    *p = 0;
    return p;
}

static char* put_str(char* p, const char* s) {
    while (*s) *p++ = *s++;
    *p = 0;
    return p;
}

static void fmt_dist(char* p, uint32_t m) {
    if (m >= 10000) {
        p = put_uint(p, (m + 500) / 1000, 1);
        put_str(p, " km");
    } else if (m >= 1000) {
        p = put_uint(p, m / 1000, 1);
        *p++ = ',';
        p = put_uint(p, (m % 1000) / 100, 1);
        put_str(p, " km");
    } else {
        p = put_uint(p, m >= 100 ? (m + 5) / 10 * 10 : (m + 2) / 5 * 5, 1);
        put_str(p, " m");
    }
}

static void fmt_hm(char* p, uint8_t h, uint8_t m) {
    p = put_uint(p, h, 2);
    *p++ = ':';
    put_uint(p, m, 2);
}

/* ---------- screens ---------- */

static void clear(void) { memset(fb, 0xFF, sizeof(fb)); }

void gui_draw_nav(const nav_data_t* nav) {
    char buf[40];
    clear();
    if (nav->flags & NAV_FLAG_ARRIVED) {
        draw_icon(NAV_ARRIVE, 52, 60, 84);
        static const char arrived[] = "\xC4\x90\xC3\xA3 \xC4\x91\xE1\xBA\xBFn n\xC6\xA1i";  // "Đã đến nơi"
        draw_text(&font_text, 112, 58, arrived, sizeof(arrived) - 1);
        return;
    }
    draw_icon(nav->icon, 50, 46, 76);

    fmt_dist(buf, nav->dist_m);
    if (text_width(&font_big, buf, strlen(buf)) <= 98)
        draw_text_center(&font_big, 50, 116, buf);
    else
        draw_text_center(&font_text, 50, 114, buf);

    fill_rect(102, 4, 2, 116);
    draw_wrapped(&font_text, 110, 17, 19, 136, 4, nav->text, nav->text_len);

    char* p = put_str(buf, "C\xC3\xB2n ");  // "Còn "
    fmt_dist(p, (uint32_t)nav->remain_10m * 10);
    p += strlen(p);
    p = put_str(p, " \xC2\xB7 ");  // " · "
    fmt_hm(p, nav->eta_h, nav->eta_m);
    draw_text(&font_small, 110, 117, buf, strlen(buf));
}

typedef struct {
    uint16_t year;
    uint8_t month, day, wday, hour, min;
} civil_t;

static civil_t to_civil(uint32_t ts) {
    civil_t c;
    uint32_t days = ts / 86400, secs = ts % 86400;
    c.hour = secs / 3600;
    c.min = secs % 3600 / 60;
    c.wday = (days + 4) % 7;  // 1970-01-01 was a Thursday
    uint32_t z = days + 719468, era = z / 146097, doe = z - era * 146097;
    uint32_t yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    uint32_t doy = doe - (365 * yoe + yoe / 4 - yoe / 100), mp = (5 * doy + 2) / 153;
    c.day = doy - (153 * mp + 2) / 5 + 1;
    c.month = mp < 10 ? mp + 3 : mp - 9;
    c.year = yoe + era * 400 + (c.month <= 2);
    return c;
}

static const char* const weekdays[] = {
    "Ch\xE1\xBB\xA7 nh\xE1\xBA\xADt",  // Chủ nhật
    "Th\xE1\xBB\xA9 Hai", "Th\xE1\xBB\xA9 Ba", "Th\xE1\xBB\xA9 T\xC6\xB0",  // Thứ Hai, Thứ Ba, Thứ Tư
    "Th\xE1\xBB\xA9 N\xC4\x83m", "Th\xE1\xBB\xA9 S\xC3\xA1u",               // Thứ Năm, Thứ Sáu
    "Th\xE1\xBB\xA9 B\xE1\xBA\xA3y",                                        // Thứ Bảy
};

static void draw_status(uint16_t mv, int8_t temp) {
    char buf[12], *p = buf;
    if (temp < 0) *p++ = '-';
    p = put_uint(p, temp < 0 ? -temp : temp, 1);
    put_str(p, "\xC2\xB0" "C");
    draw_text(&font_small, 6, 120, buf, strlen(buf));
    p = put_uint(buf, mv / 1000, 1);
    *p++ = '.';
    p = put_uint(p, mv % 1000 / 10, 2);
    put_str(p, "V");
    draw_text_right(&font_small, 244, 120, buf);
}

static bool time_synced(uint32_t ts) { return ts > 1704067200; }  // after 2024-01-01

void gui_draw_clock(uint32_t ts, uint16_t mv, int8_t temp) {
    char buf[40];
    civil_t c = to_civil(ts);
    clear();
    fmt_hm(buf, c.hour, c.min);
    draw_text_center(&font_huge, 125, 68, buf);
    if (time_synced(ts)) {
        char* p = put_str(buf, weekdays[c.wday]);
        p = put_str(p, ", ");
        p = put_uint(p, c.day, 2);
        *p++ = '/';
        p = put_uint(p, c.month, 2);
        *p++ = '/';
        put_uint(p, c.year, 4);
        draw_text_center(&font_text, 125, 96, buf);
    } else {
        draw_text_center(&font_text, 125, 96, "Ch\xC6\xB0" "a \xC4\x91\xE1\xBB\x93ng b\xE1\xBB\x99 gi\xE1\xBB\x9D");
    }
    draw_status(mv, temp);
}

void gui_draw_date(uint32_t ts, uint16_t mv, int8_t temp) {
    char buf[24];
    civil_t c = to_civil(ts);
    clear();
    put_uint(buf, c.day, 2);
    draw_text_center(&font_huge, 64, 78, buf);
    fill_rect(126, 16, 2, 80);
    draw_text(&font_text, 140, 40, weekdays[c.wday], strlen(weekdays[c.wday]));
    char* p = put_str(buf, "Th\xC3\xA1ng ");  // "Tháng "
    put_uint(p, c.month, 1);
    draw_text(&font_text, 140, 64, buf, strlen(buf));
    put_uint(buf, c.year, 4);
    draw_text(&font_text, 140, 88, buf, 4);
    draw_status(mv, temp);
}
