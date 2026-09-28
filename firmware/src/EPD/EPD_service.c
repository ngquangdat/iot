/* Copyright (c) 2012 Nordic Semiconductor. All Rights Reserved.
 *
 * The information contained herein is property of Nordic Semiconductor ASA.
 * Terms and conditions of usage are described in detail in NORDIC
 * SEMICONDUCTOR STANDARD SOFTWARE LICENSE AGREEMENT.
 *
 * Licensees are granted free, non-transferable use of the information. NO
 * WARRANTY of ANY KIND is provided. This heading must NOT be removed from
 * the file.
 *
 */

#include "EPD_service.h"

#include <string.h>

#include "app_scheduler.h"
#include "app_timer.h"
#include "ble_srv_common.h"
#include "main.h"
#include "nrf_delay.h"
#include "nrf_gpio.h"
#include "nrf_log.h"
#include "nrf_pwr_mgmt.h"
#include "sdk_macros.h"
#include "app_util_platform.h"

#if defined(S112)
#define EPD_CFG_52811 {0x14, 0x13, 0x06, 0x05, 0x04, 0x03, 0x02, 0x02, 0xFF, 0x12, 0x07}
#define EPD_CFG_52810 {0x14, 0x13, 0x12, 0x11, 0x10, 0x0F, 0x0E, 0x02, 0xFF, 0x0D, 0x02}
#else
#define EPD_CFG_DEFAULT {0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10, 0x03, 0x09, 0x03}
// #define EPD_CFG_DEFAULT {0x05, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x01, 0x07}
#endif

typedef enum { SCR_NONE = 0, SCR_NAV, SCR_CLOCK, SCR_DATE } screen_t;

// What our last render left on the panel; partial refreshes re-render it as the "old" frame.
static struct {
    screen_t screen;
    nav_data_t nav;
    uint32_t ts;
    uint16_t mv;
    int8_t temp;
    uint8_t partials;
} m_shown;

static nav_data_t m_nav_pending;
static volatile bool m_nav_active, m_nav_force, m_nav_sched;

static void render(screen_t scr, const nav_data_t* nav, uint32_t ts, uint16_t mv, int8_t temp) {
    if (scr == SCR_NAV)
        gui_draw_nav(nav);
    else if (scr == SCR_DATE)
        gui_draw_date(ts, mv, temp);
    else
        gui_draw_clock(ts, mv, temp);
}

// Report each screen update to the phone ("upd=fast 480ms"), which makes refresh issues visible in the web log.
static void report_update(ble_epd_t* p_epd, bool full, const char* why, uint32_t start_ticks) {
    uint32_t ticks = app_timer_cnt_diff_compute(app_timer_cnt_get(), start_ticks);
    uint32_t ms = ticks * 1000 / APP_TIMER_CLOCK_FREQ * (APP_TIMER_CONFIG_RTC_FREQUENCY + 1);
    char buf[40];
    snprintf(buf, sizeof(buf), "upd=%s %lums %s", full ? "full" : "fast", (unsigned long)ms, why);
    NRF_LOG_INFO("%s", buf);
    ble_epd_string_send(p_epd, (uint8_t*)buf, strlen(buf));
}

static void screen_update(ble_epd_t* p_epd, screen_t scr, const nav_data_t* nav, uint32_t ts, bool force_full) {
    uint32_t start_ticks = app_timer_cnt_get();
    EPD_GPIO_Init();
    epd_model_t* epd = epd_init((epd_model_id_t)p_epd->config.model_id);
    uint16_t mv = m_shown.mv;
    int8_t temp = m_shown.temp;
    if (scr != SCR_NAV) {
        temp = epd->drv->read_temp(epd);
        mv = EPD_ReadVoltage();
    }

    uint8_t full_every = p_epd->settings.full_every;
    const char* why = force_full                      ? "forced"
                      : p_epd->settings.fast_refresh == 0 ? "fast-off"
                      : m_shown.screen != scr           ? "new-screen"
                      : m_shown.partials >= full_every  ? "periodic"
                                                        : "";
    bool full = why[0] != 0;
    if (full) {
        render(scr, nav, ts, mv, temp);
        SSD1680_WritePlane(epd, true, gui_buffer(), GUI_BUF_SIZE);
        SSD1680_FillPlane(epd, false, 0xFF);  // no red
        epd->drv->refresh(epd);
        m_shown.partials = 0;
    } else {
        render(m_shown.screen, &m_shown.nav, m_shown.ts, m_shown.mv, m_shown.temp);
        SSD1680_WritePlane(epd, false, gui_buffer(), GUI_BUF_SIZE);
        render(scr, nav, ts, mv, temp);
        SSD1680_WritePlane(epd, true, gui_buffer(), GUI_BUF_SIZE);
        SSD1680_RefreshPartial(epd, p_epd->settings.fast_variant);
        m_shown.partials++;
    }
    m_shown.screen = scr;
    if (nav) m_shown.nav = *nav;
    m_shown.ts = ts;
    m_shown.mv = mv;
    m_shown.temp = temp;

    epd->drv->sleep(epd);
    nrf_delay_ms(200);  // for sleep
    EPD_GPIO_Uninit();
    app_feed_wdt();
    report_update(p_epd, full, why, start_ticks);
}

static void epd_gui_update(void* p_event_data, uint16_t event_size) {
    epd_gui_update_event_t* event = (epd_gui_update_event_t*)p_event_data;
    ble_epd_t* p_epd = event->p_epd;

    if (m_nav_active) return;  // navigation owns the screen
    if (p_epd->config.display_mode == MODE_PICTURE) return;
    screen_update(p_epd, p_epd->config.display_mode == MODE_CLOCK ? SCR_CLOCK : SCR_DATE, NULL, event->timestamp,
                  event->force);
}

static void epd_nav_update(void* p_event_data, uint16_t event_size) {
    epd_gui_update_event_t* event = (epd_gui_update_event_t*)p_event_data;
    static nav_data_t nav;
    bool force;

    CRITICAL_REGION_ENTER();
    m_nav_sched = false;  // later writes schedule a fresh render with the newest data
    nav = m_nav_pending;
    force = m_nav_force;
    m_nav_force = false;
    CRITICAL_REGION_EXIT();

    if (!m_nav_active) return;
    if (!force && m_shown.screen == SCR_NAV && memcmp(&nav, &m_shown.nav, sizeof(nav)) == 0) return;
    screen_update(event->p_epd, SCR_NAV, &nav, 0, force);
}

// NAV packet: 40 flags icon dist_lo dist_hi remain_lo remain_hi eta_h eta_m text...
static void epd_nav_receive(ble_epd_t* p_epd, uint8_t* p_data, uint16_t length) {
    if (length < 9) return;
    bool schedule;
    CRITICAL_REGION_ENTER();
    memset(&m_nav_pending, 0, sizeof(m_nav_pending));
    m_nav_pending.flags = p_data[1];
    m_nav_pending.icon = p_data[2];
    m_nav_pending.dist_m = p_data[3] | (p_data[4] << 8);
    m_nav_pending.remain_10m = p_data[5] | (p_data[6] << 8);
    m_nav_pending.eta_h = p_data[7];
    m_nav_pending.eta_m = p_data[8];
    m_nav_pending.text_len = MIN(length - 9, NAV_TEXT_MAX);
    memcpy(m_nav_pending.text, &p_data[9], m_nav_pending.text_len);
    if (p_data[1] & 0x01) m_nav_force = true;
    m_nav_active = true;
    schedule = !m_nav_sched;
    m_nav_sched = true;
    CRITICAL_REGION_EXIT();

    if (schedule) {
        epd_gui_update_event_t event = {p_epd, 0, false};
        app_sched_event_put(&event, sizeof(event), epd_nav_update);
    }
}

static void epd_nav_stop(ble_epd_t* p_epd) {
    if (!m_nav_active) return;
    m_nav_active = false;
    epd_gui_update_event_t event = {p_epd, timestamp(), true};
    app_sched_event_put(&event, sizeof(event), epd_gui_update);
}

/**@brief Function for handling the @ref BLE_GAP_EVT_CONNECTED event from the S110 SoftDevice.
 *
 * @param[in] p_epd     EPD Service structure.
 * @param[in] p_ble_evt Pointer to the event received from BLE stack.
 */
static void on_connect(ble_epd_t* p_epd, ble_evt_t* p_ble_evt) {
    p_epd->conn_handle = p_ble_evt->evt.gap_evt.conn_handle;
    EPD_GPIO_Init();
}

/**@brief Function for handling the @ref BLE_GAP_EVT_DISCONNECTED event from the S110 SoftDevice.
 *
 * @param[in] p_epd     EPD Service structure.
 * @param[in] p_ble_evt Pointer to the event received from BLE stack.
 */
static void on_disconnect(ble_epd_t* p_epd, ble_evt_t* p_ble_evt) {
    UNUSED_PARAMETER(p_ble_evt);
    p_epd->conn_handle = BLE_CONN_HANDLE_INVALID;
    epd_nav_stop(p_epd);
    if (p_epd->epd) {
        p_epd->epd->drv->sleep(p_epd->epd);
        nrf_delay_ms(200);  // for sleep
    }
    EPD_GPIO_Uninit();
}

static void epd_update_display_mode(ble_epd_t* p_epd, display_mode_t mode) {
    if (p_epd->config.display_mode != mode) {
        p_epd->config.display_mode = mode;
        epd_config_write(&p_epd->config);
    }
}

static void epd_send_time(ble_epd_t* p_epd) {
    char buf[20] = {0};
    snprintf(buf, 20, "t=%" PRIu32, timestamp());
    ble_epd_string_send(p_epd, (uint8_t*)buf, strlen(buf));
}

static void epd_send_mtu(ble_epd_t* p_epd) {
    char buf[40] = {0};
    snprintf(buf, sizeof(buf), "mtu=%d nav=1", p_epd->max_data_len);
    ble_epd_string_send(p_epd, (uint8_t*)buf, strlen(buf));
    snprintf(buf, sizeof(buf), "cfg fast=%d every=%d off=%d var=%d", p_epd->settings.fast_refresh,
             p_epd->settings.full_every, p_epd->settings.x_offset, p_epd->settings.fast_variant);
    ble_epd_string_send(p_epd, (uint8_t*)buf, strlen(buf));
}

static void epd_service_on_write(ble_epd_t* p_epd, uint8_t* p_data, uint16_t length) {
    NRF_LOG_DEBUG("[EPD]: on_write LEN=%d\n", length);
    NRF_LOG_HEXDUMP_DEBUG(p_data, length);
    if (p_data == NULL || length <= 0) return;

    switch (p_data[0]) {
        case EPD_CMD_SET_PINS:
            if (length < 8) return;

            p_epd->config.mosi_pin = p_data[1];
            p_epd->config.sclk_pin = p_data[2];
            p_epd->config.cs_pin = p_data[3];
            p_epd->config.dc_pin = p_data[4];
            p_epd->config.rst_pin = p_data[5];
            p_epd->config.busy_pin = p_data[6];
            p_epd->config.bs_pin = p_data[7];
            if (length > 8) p_epd->config.en_pin = p_data[8];
            epd_config_write(&p_epd->config);

            EPD_GPIO_Uninit();
            EPD_GPIO_Load(&p_epd->config);
            EPD_GPIO_Init();
            break;

        case EPD_CMD_INIT:
            p_epd->epd = epd_init((epd_model_id_t)(length > 1 ? p_data[1] : p_epd->config.model_id));
            if (p_epd->epd->id != p_epd->config.model_id) {
                p_epd->config.model_id = p_epd->epd->id;
                epd_config_write(&p_epd->config);
            }
            epd_send_mtu(p_epd);
            epd_send_time(p_epd);
            break;

        case EPD_CMD_CLEAR:
            epd_update_display_mode(p_epd, MODE_PICTURE);
            m_shown.screen = SCR_NONE;
            if (p_epd->epd) {
                p_epd->epd->drv->init(p_epd->epd);
                p_epd->epd->drv->clear(p_epd->epd, length > 1 ? p_data[1] : true);
            }
            break;

        case EPD_CMD_SEND_COMMAND:
            if (length < 2) return;
            EPD_WriteCmd(p_data[1]);
            break;

        case EPD_CMD_SEND_DATA:
            EPD_WriteData(&p_data[1], length - 1);
            break;

        case EPD_CMD_REFRESH:
            epd_update_display_mode(p_epd, MODE_PICTURE);
            m_nav_active = false;
            m_shown.screen = SCR_NONE;
            if (p_epd->epd) p_epd->epd->drv->refresh(p_epd->epd);
            break;

        case EPD_CMD_SLEEP:
            if (p_epd->epd) p_epd->epd->drv->sleep(p_epd->epd);
            break;

        case EPD_CMD_SET_TIME: {
            if (length < 5) return;

            NRF_LOG_DEBUG("time: %02x %02x %02x %02x\n", p_data[1], p_data[2], p_data[3], p_data[4]);
            if (length > 5) NRF_LOG_DEBUG("timezone: %d\n", (int8_t)p_data[5]);

            uint32_t timestamp = (p_data[1] << 24) | (p_data[2] << 16) | (p_data[3] << 8) | p_data[4];
            timestamp += (length > 5 ? (int8_t)p_data[5] : 8) * 60 * 60;  // timezone
            set_timestamp(timestamp);
            m_nav_active = false;
            epd_update_display_mode(p_epd, length > 6 ? (display_mode_t)(p_data[6] & 0x7F) : MODE_CALENDAR);
            ble_epd_on_timer(p_epd, timestamp, true);
        } break;

        case EPD_CMD_SET_WEEK_START:
            if (length < 2) return;
            if (p_data[1] < 7 && p_data[1] != p_epd->config.week_start) {
                p_epd->config.week_start = p_data[1];
                epd_config_write(&p_epd->config);
            }
            break;

        case EPD_CMD_WRITE_IMAGE: {
            if (length < 3) return;

            // low nibble 0x0F: black plane (else red), high nibble 0xF0: continuation (else begin)
            bool black = (p_data[1] & 0x0F) == 0x0F;
            bool begin = (p_data[1] & 0xF0) == 0x00;
            if (p_epd->epd) p_epd->epd->drv->write_ram(p_epd->epd, begin, black, &p_data[2], length - 2);
        } break;

        case EPD_CMD_NAV:
            epd_nav_receive(p_epd, p_data, length);
            break;

        case EPD_CMD_NAV_CTRL:
            if (length < 2) return;
            if (p_data[1] == 0x00) {
                epd_nav_stop(p_epd);
            } else if (length > 2 && (p_data[1] == 0x01 || p_data[1] == 0x02)) {
                if (p_data[1] == 0x01)
                    p_epd->settings.fast_refresh = p_data[2] ? 1 : 0;
                else
                    p_epd->settings.full_every = p_data[2] == 0 ? 1 : (p_data[2] > 100 ? 100 : p_data[2]);
                epd_settings_write(&p_epd->settings);
            } else if (length > 2 && p_data[1] == 0x04 && p_data[2] <= 2) {
                p_epd->settings.fast_variant = p_data[2];
                epd_settings_write(&p_epd->settings);
            } else if (length > 2 && p_data[1] == 0x03 && p_data[2] <= 6) {
                p_epd->settings.x_offset = p_data[2];
                SSD1680_SetXOffset(p_data[2]);
                epd_settings_write(&p_epd->settings);
                m_shown.screen = SCR_NONE;  // next update redraws everything at the new position
            }
            break;

        case EPD_CMD_SET_CONFIG:
            if (length < 2) return;
            memcpy(&p_epd->config, &p_data[1], (length - 1 > EPD_CONFIG_SIZE) ? EPD_CONFIG_SIZE : length - 1);
            epd_config_write(&p_epd->config);
            break;

        case EPD_CMD_SYS_SLEEP:
            sleep_mode_enter();
            break;

        case EPD_CMD_SYS_RESET:
#if defined(S112)
            nrf_pwr_mgmt_shutdown(NRF_PWR_MGMT_SHUTDOWN_RESET);
#else
            NVIC_SystemReset();
#endif
            break;

        case EPD_CMD_CFG_ERASE:
            epd_config_clear(&p_epd->config);
            nrf_delay_ms(100);  // required
            NVIC_SystemReset();
            break;

        default:
            break;
    }
}

/**@brief Function for handling the @ref BLE_GATTS_EVT_WRITE event from the S110 SoftDevice.
 *
 * @param[in] p_epd     EPD Service structure.
 * @param[in] p_ble_evt Pointer to the event received from BLE stack.
 */
static void on_write(ble_epd_t* p_epd, ble_evt_t* p_ble_evt) {
    ble_gatts_evt_write_t* p_evt_write = &p_ble_evt->evt.gatts_evt.params.write;

    if ((p_evt_write->handle == p_epd->char_handles.cccd_handle) && (p_evt_write->len == 2)) {
        if (ble_srv_is_notification_enabled(p_evt_write->data)) {
            NRF_LOG_DEBUG("notification enabled\n");
            p_epd->is_notification_enabled = true;
            static uint16_t length = EPD_CONFIG_NOTIFY_SIZE;
            NRF_LOG_DEBUG("send epd config\n");
            uint32_t err_code = ble_epd_string_send(p_epd, (uint8_t*)&p_epd->config, length);
            if (err_code != NRF_ERROR_INVALID_STATE) APP_ERROR_CHECK(err_code);
        } else {
            p_epd->is_notification_enabled = false;
        }
    } else if (p_evt_write->handle == p_epd->char_handles.value_handle) {
        epd_service_on_write(p_epd, p_evt_write->data, p_evt_write->len);
    } else {
        // Do Nothing. This event is not relevant for this service.
    }
}

#if defined(S112)
void ble_epd_evt_handler(ble_evt_t const* p_ble_evt, void* p_context) {
    if (p_context == NULL || p_ble_evt == NULL) return;

    ble_epd_t* p_epd = (ble_epd_t*)p_context;
    ble_epd_on_ble_evt(p_epd, (ble_evt_t*)p_ble_evt);
}
#endif

void ble_epd_on_ble_evt(ble_epd_t* p_epd, ble_evt_t* p_ble_evt) {
    if ((p_epd == NULL) || (p_ble_evt == NULL)) {
        return;
    }

    switch (p_ble_evt->header.evt_id) {
        case BLE_GAP_EVT_CONNECTED:
            on_connect(p_epd, p_ble_evt);
            break;

        case BLE_GAP_EVT_DISCONNECTED:
            on_disconnect(p_epd, p_ble_evt);
            break;

        case BLE_GATTS_EVT_WRITE:
            on_write(p_epd, p_ble_evt);
            break;

        default:
            // No implementation needed.
            break;
    }
}

static uint32_t epd_service_init(ble_epd_t* p_epd) {
    ble_uuid_t ble_uuid = {0};
    ble_uuid128_t base_uuid = BLE_UUID_EPD_SVC_BASE;
    ble_add_char_params_t add_char_params;
    uint8_t app_version = APP_VERSION;

    VERIFY_SUCCESS(sd_ble_uuid_vs_add(&base_uuid, &ble_uuid.type));

    ble_uuid.type = ble_uuid.type;
    ble_uuid.uuid = BLE_UUID_EPD_SVC;
    VERIFY_SUCCESS(sd_ble_gatts_service_add(BLE_GATTS_SRVC_TYPE_PRIMARY, &ble_uuid, &p_epd->service_handle));

    memset(&add_char_params, 0, sizeof(add_char_params));
    add_char_params.uuid = BLE_UUID_EPD_CHAR;
    add_char_params.uuid_type = ble_uuid.type;
    add_char_params.max_len = BLE_EPD_MAX_DATA_LEN;
    add_char_params.init_len = sizeof(uint8_t);
    add_char_params.is_var_len = true;
    add_char_params.char_props.notify = 1;
    add_char_params.char_props.write = 1;
    add_char_params.char_props.write_wo_resp = 1;
    add_char_params.read_access = SEC_OPEN;
    add_char_params.write_access = SEC_OPEN;
    add_char_params.cccd_write_access = SEC_OPEN;

    VERIFY_SUCCESS(characteristic_add(p_epd->service_handle, &add_char_params, &p_epd->char_handles));

    memset(&add_char_params, 0, sizeof(add_char_params));
    add_char_params.uuid = BLE_UUID_APP_VER;
    add_char_params.uuid_type = ble_uuid.type;
    add_char_params.max_len = sizeof(uint8_t);
    add_char_params.init_len = sizeof(uint8_t);
    add_char_params.p_init_value = &app_version;
    add_char_params.char_props.read = 1;
    add_char_params.read_access = SEC_OPEN;

    return characteristic_add(p_epd->service_handle, &add_char_params, &p_epd->app_ver_handles);
}

void ble_epd_sleep_prepare(ble_epd_t* p_epd) {
    // Turn off led
    EPD_LED_OFF();
    // Prepare wakeup pin
    if (p_epd->config.wakeup_pin != 0xFF) {
        nrf_gpio_cfg_sense_input(p_epd->config.wakeup_pin, NRF_GPIO_PIN_NOPULL, NRF_GPIO_PIN_SENSE_HIGH);
    }
}

uint32_t ble_epd_init(ble_epd_t* p_epd) {
    if (p_epd == NULL) return NRF_ERROR_NULL;

    // Initialize the service structure.
    p_epd->max_data_len = BLE_EPD_MAX_DATA_LEN;
    p_epd->conn_handle = BLE_CONN_HANDLE_INVALID;
    p_epd->is_notification_enabled = false;

    epd_config_init(&p_epd->config);
    epd_config_read(&p_epd->config);

    // write default config
    if (epd_config_empty(&p_epd->config)) {
#if defined(S112)
        if (NRF_FICR->INFO.PART == 0x52810) {
            uint8_t cfg[] = EPD_CFG_52810;
            memcpy(&p_epd->config, cfg, sizeof(cfg));
        } else {
            uint8_t cfg[] = EPD_CFG_52811;
            memcpy(&p_epd->config, cfg, sizeof(cfg));
        }
#else
        uint8_t cfg[] = EPD_CFG_DEFAULT;
        memcpy(&p_epd->config, cfg, sizeof(cfg));
#endif
        if (p_epd->config.display_mode == 0xFF) p_epd->config.display_mode = MODE_CALENDAR;
        if (p_epd->config.week_start == 0xFF) p_epd->config.week_start = 0;
        epd_config_write(&p_epd->config);
    }

    p_epd->config.panel = 0x21;
    epd_settings_read(&p_epd->settings);
    SSD1680_SetXOffset(p_epd->settings.x_offset);
    if (p_epd->config.model_id != SSD1680_213_BWR && p_epd->config.model_id != SSD1680_213_BW)
        p_epd->config.model_id = SSD1680_213_BWR;

    // load config
    EPD_GPIO_Load(&p_epd->config);

    // blink LED on start
    EPD_LED_BLINK();

    // Add the service.
    return epd_service_init(p_epd);
}

uint32_t ble_epd_string_send(ble_epd_t* p_epd, uint8_t* p_string, uint16_t length) {
    if ((p_epd->conn_handle == BLE_CONN_HANDLE_INVALID) || (!p_epd->is_notification_enabled))
        return NRF_ERROR_INVALID_STATE;
    if (length > p_epd->max_data_len) return NRF_ERROR_INVALID_PARAM;

    ble_gatts_hvx_params_t hvx_params;

    memset(&hvx_params, 0, sizeof(hvx_params));

    hvx_params.handle = p_epd->char_handles.value_handle;
    hvx_params.p_data = p_string;
    hvx_params.p_len = &length;
    hvx_params.type = BLE_GATT_HVX_NOTIFICATION;

    return sd_ble_gatts_hvx(p_epd->conn_handle, &hvx_params);
}

void ble_epd_on_timer(ble_epd_t* p_epd, uint32_t timestamp, bool force_update) {
    // Update calendar on 00:00:00, clock on every minute
    if (force_update || (p_epd->config.display_mode == MODE_CALENDAR && timestamp % 86400 == 0) ||
        (p_epd->config.display_mode == MODE_CLOCK && timestamp % 60 == 0)) {
        epd_gui_update_event_t event = {p_epd, timestamp, force_update};
        app_sched_event_put(&event, sizeof(epd_gui_update_event_t), epd_gui_update);
    }
}
