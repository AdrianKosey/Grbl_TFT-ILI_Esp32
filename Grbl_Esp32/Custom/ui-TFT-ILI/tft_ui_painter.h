#pragma once

#include <Arduino.h>
#include <TFT_eSPI.h>

namespace tft_ui {

static constexpr uint16_t WIDTH_TOPBAR        = 280;
static constexpr uint16_t HEIGHT_TOPBAR       = 30;
static constexpr uint16_t WIDTH_SIDEBAR       = 40;
static constexpr uint16_t HEIGHT_SIDEBAR      = 240;
static constexpr uint16_t WIDTH_TOPBAR_BUTTON = 20;
static constexpr uint16_t HEIGHT_TOPBAR_BUTTON = 20;

struct TouchButton {
    uint16_t    x1, y1, x2, y2;
    const char* label;
    const char* iconPath;
    const char* iconPathActive;
    uint16_t    color;
};

inline uint16_t color565(uint8_t r, uint8_t g, uint8_t b) {
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

static constexpr uint16_t SIDEBAR_COLOR              = ((0x18 & 0xF8) << 8) | ((0x1B & 0xFC) << 3) | (0x23 >> 3);
static constexpr uint16_t TOPBAR_COLOR               = ((0x18 & 0xF8) << 8) | ((0x1B & 0xFC) << 3) | (0x23 >> 3);
static constexpr uint16_t BG_COLOR                   = ((0x22 & 0xF8) << 8) | ((0x25 & 0xFC) << 3) | (0x30 >> 3);
static constexpr uint16_t STATUS_BAR_COLOR           = ((0x9f & 0xF8) << 8) | ((0xa7 & 0xFC) << 3) | (0xa8 >> 3);
static constexpr uint16_t MAIN_BUTTON_COLOR          = ((0x3d & 0xF8) << 8) | ((0x50 & 0xFC) << 3) | (0x93 >> 3);
static constexpr uint16_t PROGRESS_BAR_BACKGROUND    = ((0x88 & 0xF8) << 8) | ((0x88 & 0xFC) << 3) | (0x88 >> 3);
static constexpr uint16_t PROGRESS_BAR_FILL          = ((0x69 & 0xF8) << 8) | ((0xe3 & 0xFC) << 3) | (0xf3 >> 3);
static constexpr uint16_t CONTAINER_COORDS_COLOR     = ((0x18 & 0xF8) << 8) | ((0x1b & 0xFC) << 3) | (0x23 >> 3);
static constexpr uint16_t CONTAINER_BORD_COORDS_COLOR = ((0x69 & 0xF8) << 8) | ((0xe3 & 0xFC) << 3) | (0xf3 >> 3);

inline void draw_button(TFT_eSPI& tft,
                        const TouchButton& button,
                        bool active,
                        void (*drawSdJpegFn)(const char*, int, int)) {
    tft.fillRect(button.x1, button.y1, button.x2 - button.x1, button.y2 - button.y1, button.color);
    drawSdJpegFn(active ? button.iconPathActive : button.iconPath, button.x1, button.y1);
}

inline void draw_menu_shell(TFT_eSPI& tft, const char* title) {
    tft.fillScreen(BG_COLOR);
    tft.fillRect(40, 0, WIDTH_TOPBAR, HEIGHT_TOPBAR, TOPBAR_COLOR);
    tft.setTextSize(1);
    tft.setTextColor(TFT_WHITE, TOPBAR_COLOR);
    tft.setTextDatum(MC_DATUM);
    tft.drawString(title, tft.width() / 2, HEIGHT_TOPBAR / 2);
    tft.fillRect(0, 0, WIDTH_SIDEBAR, HEIGHT_SIDEBAR, SIDEBAR_COLOR);
}

inline void draw_ctrl_btn(TFT_eSPI& tft, int x, int y, int w, int h, const char* label) {
    tft.fillRect(x, y, w, h, CONTAINER_COORDS_COLOR);
    tft.drawRect(x, y, w, h, CONTAINER_BORD_COORDS_COLOR);
    tft.setTextSize(1);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(CONTAINER_BORD_COORDS_COLOR, CONTAINER_COORDS_COLOR);
    tft.drawString(label, x + (w / 2), y + (h / 2));
}

}  // namespace tft_ui::paint