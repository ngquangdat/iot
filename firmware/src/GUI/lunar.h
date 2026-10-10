#ifndef __LUNAR_H
#define __LUNAR_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    uint16_t year;  // lunar year (Tet falls in this solar year)
    uint8_t month;  // 1..12
    uint8_t day;    // 1..30
    bool leap;      // leap (nhuận) month
} lunar_date_t;

extern const uint16_t lunar_first_year;
extern const uint16_t lunar_year_count;
extern const uint32_t lunar_years[];

// days: days since 1970-01-01 (local time). Returns false outside the table (2024..2063).
bool lunar_from_days(uint32_t days, lunar_date_t* out);
// "Canh Tý" style name of a lunar year.
const char* lunar_can(uint16_t year);
const char* lunar_chi(uint16_t year);

#endif
