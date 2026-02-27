#pragma once

#include <Arduino.h>
#include <TFT_eSPI.h>

namespace tft_ui {

struct TouchEvent {
    bool     pressed = false;
    uint16_t x       = 0;
    uint16_t y       = 0;
};

class TouchReader {
public:
    TouchEvent read(TFT_eSPI& tft) {
        TouchEvent event;
        uint16_t   tx = 0;
        uint16_t   ty = 0;
        bool       now_pressed = tft.getTouch(&tx, &ty);

        if (!now_pressed) {
            _was_pressed = false;
            return event;
        }

        unsigned long now = millis();
        if (_was_pressed && (now - _last_press_ms) < _debounce_ms) {
            return event;
        }

        _was_pressed   = true;
        _last_press_ms = now;

        event.pressed = true;
        event.x       = tx;
        event.y       = tft.height() - ty;  // Normalizar origen arriba-izquierda
        return event;
    }

private:
    bool          _was_pressed   = false;
    unsigned long _last_press_ms = 0;
    const unsigned long _debounce_ms = 120;
};

}  // namespace tft_ui