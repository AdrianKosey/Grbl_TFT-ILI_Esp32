#pragma once

namespace tft_ui {

enum UIState { UI_MENU, UI_HOME, UI_MEDIA, UI_CONTROL, UI_CONFIG, NO_CHANGE };

inline bool should_redraw_screen(UIState current, UIState next) {
    return current != next && next != NO_CHANGE;
}

}  // namespace tft_ui