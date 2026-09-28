#include "EPD_driver.h"

// SSD1680 driver for the 2.13" 122x250 panel of the AESL0213C.
//
// RAM layout: 16 bytes (128 sources) per gate line, 250 gate lines. The host
// streams frames column-first from the landscape image's right edge, which
// maps directly onto this layout with entry mode 0x03.

#define GATES 250

bool SSD16xx_ReadBusy(epd_model_t* epd) { return EPD_ReadBusy(); }

static void SSD16xx_WaitBusy(uint16_t timeout) { EPD_WaitBusy(true, timeout); }

static void SSD16xx_Update(uint8_t seq) {
    EPD_Write(SSD16xx_DISP_CTRL2, seq);
    EPD_WriteCmd(SSD16xx_MASTER_ACTIVATE);
}

int8_t SSD16xx_ReadTemp(epd_model_t* epd) {
    SSD16xx_Update(0xB1);
    SSD16xx_WaitBusy(500);
    EPD_WriteCmd(SSD16xx_TSENSOR_READ);
    return (int8_t)EPD_ReadByte();
}

static void SSD16xx_SetWindow(epd_model_t* epd) {
    uint16_t last = epd->height - 1;
    EPD_Write(SSD16xx_ENTRY_MODE, 0x03);  // x increase, y increase
    EPD_Write(SSD16xx_RAM_XPOS, 0x00, (epd->width / 8) - 1);
    EPD_Write(SSD16xx_RAM_YPOS, 0x00, 0x00, last % 256, last / 256);
    EPD_Write(SSD16xx_RAM_XCOUNT, 0x00);
    EPD_Write(SSD16xx_RAM_YCOUNT, 0x00, 0x00);
}

void SSD16xx_Init(epd_model_t* epd) {
    EPD_Reset(true, 10);

    EPD_WriteCmd(SSD16xx_SW_RESET);
    SSD16xx_WaitBusy(200);

    EPD_Write(SSD16xx_GDO_CTR, (GATES - 1) % 256, (GATES - 1) / 256, 0x00);
    EPD_Write(SSD16xx_BORDER_CTRL, 0x05);
    EPD_Write(SSD16xx_TSENSOR_CTRL, 0x80);

    SSD16xx_SetWindow(epd);
}

static void SSD16xx_Refresh(epd_model_t* epd) {
    // BWR: red RAM is inverted, so the host sends 0 for red pixels.
    EPD_Write(SSD16xx_DISP_CTRL1, epd->color == COLOR_BWR ? 0x80 : 0x40, 0x00);
    SSD16xx_Update(0xF7);
    SSD16xx_WaitBusy(UINT16_MAX);
    SSD16xx_SetWindow(epd);  // DO NOT REMOVE!
}

void SSD16xx_Clear(epd_model_t* epd, bool refresh) {
    SSD1680_FillPlane(epd, true, 0xFF);
    SSD1680_FillPlane(epd, false, 0xFF);
    if (refresh) SSD16xx_Refresh(epd);
}

void SSD16xx_WriteImage(epd_model_t* epd, uint8_t* black, uint8_t* color, uint16_t x, uint16_t y, uint16_t w,
                        uint16_t h) {
    // Unused: the on-device screens render full frames with SSD1680_WritePlane.
}

void SSD16xx_WriteRam(epd_model_t* epd, bool begin, bool black, uint8_t* data, uint8_t len) {
    if (begin && black) SSD16xx_SetWindow(epd);
    if (begin) {
        if (epd->color == COLOR_BWR)
            EPD_WriteCmd(black ? SSD16xx_WRITE_RAM1 : SSD16xx_WRITE_RAM2);
        else
            EPD_WriteCmd(SSD16xx_WRITE_RAM1);
    }
    EPD_WriteData(data, len);
}

void SSD16xx_Sleep(epd_model_t* epd) {
    EPD_Write(SSD16xx_SLEEP_MODE, 0x01);
    delay(100);
}

void SSD1680_WritePlane(epd_model_t* epd, bool bw_ram, const uint8_t* data, uint16_t len) {
    SSD16xx_SetWindow(epd);
    EPD_WriteCmd(bw_ram ? SSD16xx_WRITE_RAM1 : SSD16xx_WRITE_RAM2);
    while (len > 0) {
        uint8_t n = len > 250 ? 250 : (uint8_t)len;
        EPD_WriteData((uint8_t*)data, n);
        data += n;
        len -= n;
    }
}

void SSD1680_FillPlane(epd_model_t* epd, bool bw_ram, uint8_t value) {
    SSD16xx_SetWindow(epd);
    EPD_FillRAM(bw_ram ? SSD16xx_WRITE_RAM1 : SSD16xx_WRITE_RAM2, value, (epd->width / 8) * epd->height);
}

// Fast black/white waveform (SSD1680 LUT format: 5x12 VS, 12x7 TP/RP, 6 FR, 3 XON)
// followed by EOPT, VGH, VSH1, VSH2, VSL and VCOM. The panel is tri-color and has
// no OTP fast mode, so this is loaded by the host. Experimental on BWR glass:
// periodic full refreshes clean up the ghosting it leaves behind.
static const uint8_t lut_fast_bw[159] = {
    0x00, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // LUT0: black -> black
    0x80, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // LUT1: black -> white
    0x40, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // LUT2: white -> black
    0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // LUT3: white -> white
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // LUT4: VCOM
    0x14, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // group 0
    0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // group 1
    0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // group 2
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // group 3
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // group 4
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // group 5
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // group 6
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // group 7
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // group 8
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // group 9
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // group 10
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // group 11
    0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x00, 0x00, 0x00,  // FR, XON
    0x22, 0x17, 0x41, 0x00, 0x32, 0x36,                    // EOPT, VGH, VSH1, VSH2, VSL, VCOM
};

// Partial refresh: the BW RAM holds the new frame, the RED RAM the frame
// currently on screen; Display Mode 2 drives only pixels that change.
void SSD1680_RefreshPartial(epd_model_t* epd) {
    EPD_WriteCmd(SSD16xx_WRITE_LUT);
    EPD_WriteData((uint8_t*)lut_fast_bw, 153);
    EPD_Write(0x3F, lut_fast_bw[153]);
    EPD_Write(SSD16xx_GDV_CTRL, lut_fast_bw[154]);
    EPD_Write(SSD16xx_SDV_CTRL, lut_fast_bw[155], lut_fast_bw[156], lut_fast_bw[157]);
    EPD_Write(SSD16xx_VCOM_VOLTAGE, lut_fast_bw[158]);
    EPD_Write(SSD16xx_BORDER_CTRL, 0x80);
    EPD_Write(SSD16xx_DISP_CTRL1, 0x00, 0x00);  // both RAMs as-is
    SSD16xx_Update(0xCF);                        // clock+analog on, Display Mode 2, off afterwards
    SSD16xx_WaitBusy(5000);
    SSD16xx_SetWindow(epd);
}

static const epd_driver_t epd_drv_ssd16xx = {
    .init = SSD16xx_Init,
    .clear = SSD16xx_Clear,
    .write_image = SSD16xx_WriteImage,
    .write_ram = SSD16xx_WriteRam,
    .refresh = SSD16xx_Refresh,
    .sleep = SSD16xx_Sleep,
    .read_temp = SSD16xx_ReadTemp,
    .read_busy = SSD16xx_ReadBusy,
};

// Native orientation: 128 sources x 250 gates (landscape 250x128 on screen).
const epd_model_t epd_ssd1680_213_bwr = {SSD1680_213_BWR, COLOR_BWR, &epd_drv_ssd16xx, DRV_IC_SSD1680, 128, GATES};
const epd_model_t epd_ssd1680_213_bw = {SSD1680_213_BW, COLOR_BW, &epd_drv_ssd16xx, DRV_IC_SSD1680, 128, GATES};
