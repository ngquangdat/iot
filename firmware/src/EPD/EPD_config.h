#ifndef __EPD_CONFIG_H__
#define __EPD_CONFIG_H__
#include <stdbool.h>
#include <stdint.h>

typedef struct {
    uint8_t mosi_pin;
    uint8_t sclk_pin;
    uint8_t cs_pin;
    uint8_t dc_pin;
    uint8_t rst_pin;
    uint8_t busy_pin;
    uint8_t bs_pin;
    uint8_t model_id;
    uint8_t wakeup_pin;
    uint8_t led_pin;
    uint8_t en_pin;
    uint8_t display_mode;
    uint8_t week_start;
    uint8_t panel;  // 0x21 = 2.13" (reported to the web tools like the stock firmware)
} epd_config_t;

#define EPD_CONFIG_NOTIFY_SIZE 14  // what the web tools expect on connect

// Our own settings live in a separate record: the stock firmware's config record
// is longer than ours and its extra bytes must not be mistaken for these.
typedef struct {
    uint8_t magic;         // SETTINGS_MAGIC
    uint8_t fast_refresh;  // 0: always full refresh, 1: partial updates for clock/nav
    uint8_t full_every;    // full refresh after this many partial updates (1..100)
    uint8_t x_offset;      // RAM column offset in bytes (the glass starts at source 8*x_offset)
    uint8_t fast_variant;  // fast refresh method, see SSD1680_RefreshPartial (0..2)
    uint8_t pad[3];
} epd_settings_t;

#define SETTINGS_MAGIC 0x4E  // 'N'

void epd_settings_read(epd_settings_t* s);  // loads and sanitizes, falling back to defaults
void epd_settings_write(epd_settings_t* s);

#define EPD_CONFIG_SIZE (sizeof(epd_config_t) / sizeof(uint8_t))

void epd_config_init(epd_config_t* cfg);
void epd_config_read(epd_config_t* cfg);
void epd_config_write(epd_config_t* cfg);
void epd_config_clear(epd_config_t* cfg);
bool epd_config_empty(epd_config_t* cfg);

#endif
