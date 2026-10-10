#include "lunar.h"

// Days since 1970-01-01 of a civil date (Howard Hinnant's algorithm).
static uint32_t days_from_civil(uint32_t y, uint32_t m, uint32_t d) {
    y -= m <= 2;
    uint32_t era = y / 400, yoe = y - era * 400;
    uint32_t doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    uint32_t doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + doe - 719468;
}

static uint32_t tet_days(uint16_t year) {
    uint32_t e = lunar_years[year - lunar_first_year];
    return days_from_civil(year, 1, 1) + ((e >> 17) & 0x3F);
}

bool lunar_from_days(uint32_t days, lunar_date_t* out) {
    // Solar year of the date: estimate, then correct.
    uint16_t y = 1970 + days / 366;
    while (days_from_civil(y + 1, 1, 1) <= days) y++;
    if (y < lunar_first_year || y >= lunar_first_year + lunar_year_count) return false;
    if (days < tet_days(y)) {
        if (y == lunar_first_year) return false;
        y--;
    }
    uint32_t e = lunar_years[y - lunar_first_year];
    uint8_t leap = (e >> 13) & 0x0F;
    uint32_t off = days - tet_days(y);
    uint8_t month = 1, idx = 0;
    bool is_leap = false;
    for (;;) {
        uint8_t len = (e >> idx) & 1 ? 30 : 29;
        if (off < len) break;
        off -= len;
        idx++;
        if (leap && !is_leap && month == leap) {
            is_leap = true;  // the leap month follows its normal month
        } else {
            is_leap = false;
            month++;
        }
        if (month > 12) return false;
    }
    out->year = y;
    out->month = month;
    out->day = off + 1;
    out->leap = is_leap;
    return true;
}

static const char* const can[] = {"Canh", "T\xC3\xA2n", "Nh\xC3\xA2m", "Qu\xC3\xBD", "Gi\xC3\xA1p",
                                  "\xE1\xBA\xA4t", "B\xC3\xADnh", "\xC4\x90inh", "M\xE1\xBA\xADu", "K\xE1\xBB\xB7"};
static const char* const chi[] = {"Th\xC3\xA2n", "D\xE1\xBA\xADu", "Tu\xE1\xBA\xA5t", "H\xE1\xBB\xA3i",
                                  "T\xC3\xBD", "S\xE1\xBB\xADu", "D\xE1\xBA\xA7n", "M\xC3\xA3o",
                                  "Th\xC3\xACn", "T\xE1\xBB\xB5", "Ng\xE1\xBB\x8D", "M\xC3\xB9i"};

const char* lunar_can(uint16_t year) { return can[year % 10]; }
const char* lunar_chi(uint16_t year) { return chi[year % 12]; }
