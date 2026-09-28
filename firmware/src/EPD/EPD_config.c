#include "EPD_config.h"

#include <string.h>

#include "app_scheduler.h"
#include "fds.h"
#include "nordic_common.h"
#include "nrf_log.h"

#define CONFIG_FILE_ID 0x0000
#define CONFIG_REC_KEY 0x0001

#define SETTINGS_REC_KEY 0x0002

// A write that failed for lack of space is retried once garbage collection finishes.
static struct {
    const void* data;
    uint16_t size;
} m_pending[2];

static void record_write(uint16_t key, const void* data, uint16_t size);

static void run_fds_gc(void* p_event_data, uint16_t event_size) {
    NRF_LOG_DEBUG("run garbage collection (fds_gc)\n");
    fds_gc();
}

static void fds_evt_handler(fds_evt_t const* const p_fds_evt) {
    NRF_LOG_DEBUG("fds evt: id=%d result=%d\n", p_fds_evt->id, p_fds_evt->result);
    if (p_fds_evt->id == FDS_EVT_GC && p_fds_evt->result == NRF_SUCCESS) {
        for (uint8_t i = 0; i < 2; i++) {
            if (m_pending[i].data == NULL) continue;
            const void* data = m_pending[i].data;
            m_pending[i].data = NULL;
            record_write(i == 0 ? CONFIG_REC_KEY : SETTINGS_REC_KEY, data, m_pending[i].size);
        }
    }
}

void epd_config_init(epd_config_t* cfg) {
    ret_code_t ret;

    ret = fds_register(fds_evt_handler);
    if (ret != NRF_SUCCESS) {
        NRF_LOG_ERROR("fds_register failed, code=%d\n", ret);
        return;
    }

    ret = fds_init();
    if (ret != NRF_SUCCESS) {
        NRF_LOG_ERROR("fds_init failed, code=%d\n", ret);
        return;
    }

    run_fds_gc(NULL, 0);
}

static bool record_read(uint16_t key, void* data, uint16_t size) {
    fds_flash_record_t flash_record;
    fds_record_desc_t record_desc;
    fds_find_token_t ftok;

    memset(data, 0xFF, size);
    memset(&ftok, 0x00, sizeof(fds_find_token_t));
    if (fds_record_find(CONFIG_FILE_ID, key, &record_desc, &ftok) != NRF_SUCCESS) return false;
    if (fds_record_open(&record_desc, &flash_record) != NRF_SUCCESS) return false;
#ifdef S112
    uint32_t record_len = flash_record.p_header->length_words * sizeof(uint32_t);
#else
    uint32_t record_len = flash_record.p_header->tl.length_words * sizeof(uint32_t);
#endif
    memcpy(data, flash_record.p_data, MIN(size, record_len));
    fds_record_close(&record_desc);
    return true;
}

static void record_write(uint16_t key, const void* data, uint16_t size) {
    ret_code_t ret;
    fds_record_t record;
    fds_record_desc_t record_desc;
    fds_find_token_t ftok;

    record.file_id = CONFIG_FILE_ID;
    record.key = key;
#ifdef S112
    record.data.p_data = data;
    record.data.length_words = BYTES_TO_WORDS(size);
#else
    fds_record_chunk_t record_chunk;
    record_chunk.p_data = data;
    record_chunk.length_words = BYTES_TO_WORDS(size);
    record.data.p_chunks = &record_chunk;
    record.data.num_chunks = 1;
#endif

    memset(&ftok, 0x00, sizeof(fds_find_token_t));
    ret = fds_record_find(CONFIG_FILE_ID, key, &record_desc, &ftok);
    if (ret == NRF_SUCCESS)
        ret = fds_record_update(&record_desc, &record);
    else
        ret = fds_record_write(&record_desc, &record);

    if (ret == FDS_ERR_NO_SPACE_IN_FLASH) {
        NRF_LOG_ERROR("record write: no space, retry after gc\n");
        uint8_t slot = key == CONFIG_REC_KEY ? 0 : 1;
        m_pending[slot].data = data;
        m_pending[slot].size = size;
        app_sched_event_put(NULL, 0, run_fds_gc);
    } else if (ret != NRF_SUCCESS) {
        NRF_LOG_ERROR("record write failed, code=%d\n", ret);
    }
}

void epd_config_read(epd_config_t* cfg) { record_read(CONFIG_REC_KEY, cfg, sizeof(epd_config_t)); }

void epd_config_write(epd_config_t* cfg) { record_write(CONFIG_REC_KEY, cfg, sizeof(epd_config_t)); }

void epd_settings_read(epd_settings_t* st) {
    bool found = record_read(SETTINGS_REC_KEY, st, sizeof(epd_settings_t));
    bool valid = found && st->magic == SETTINGS_MAGIC;
    if (!valid) {
        st->magic = SETTINGS_MAGIC;
        st->fast_refresh = 1;
        st->full_every = 20;
        st->x_offset = 1;
        st->fast_variant = 0;
    }
    if (st->fast_refresh > 1) st->fast_refresh = 1;
    if (st->full_every == 0 || st->full_every > 100) st->full_every = 20;
    if (st->x_offset > 6) st->x_offset = 1;
    if (st->fast_variant > 2) st->fast_variant = 0;
    if (!valid) epd_settings_write(st);
}

void epd_settings_write(epd_settings_t* st) { record_write(SETTINGS_REC_KEY, st, sizeof(epd_settings_t)); }

void epd_config_clear(epd_config_t* cfg) {
    ret_code_t ret;
    fds_record_desc_t record_desc;
    fds_find_token_t ftok;

    memset(&ftok, 0x00, sizeof(fds_find_token_t));
    if (fds_record_find(CONFIG_FILE_ID, CONFIG_REC_KEY, &record_desc, &ftok) != NRF_SUCCESS) {
        NRF_LOG_DEBUG("epd_config_clear: record not found\n");
        return;
    }

    ret = fds_record_delete(&record_desc);
    if (ret != NRF_SUCCESS) {
        NRF_LOG_ERROR("fds_record_delete failed, code=%d\n", ret);
    }
}

bool epd_config_empty(epd_config_t* cfg) {
    for (uint8_t i = 0; i < EPD_CONFIG_SIZE; i++) {
        if (((uint8_t*)cfg)[i] != 0xFF) return false;
    }
    return true;
}
