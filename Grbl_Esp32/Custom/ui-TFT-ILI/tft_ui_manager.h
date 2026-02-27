#pragma once

#include "tft_ui_core.h"

namespace tft_ui {

class Manager {
public:
    static bool can_enter_work_screen() {
        return get_sd_state(false) == SDState::BusyPrinting;
    }

    static void on_job_started(RuntimeState& state) {
        state.job_start_time        = millis();
        state.total_pause_time      = 0;
        state.pause_start_timestamp = 0;
        state.timer_paused          = false;
    }

    static bool on_stop_requested(bool& sd_present) {
        sd_present = Core::safe_stop_job();
        return sd_present;
    }
};

}  // namespace tft_ui