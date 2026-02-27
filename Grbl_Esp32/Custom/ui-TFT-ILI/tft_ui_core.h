#pragma once

#include "tft_ui_protocol.h"
#include <string.h>
#include "../../Grbl_Esp32/Grbl_Esp32/src/Grbl.h"
namespace tft_ui {

portMUX_TYPE myMutex = portMUX_INITIALIZER_UNLOCKED;


struct RuntimeState {
    unsigned long job_start_time        = 0;
    unsigned long total_pause_time      = 0;
    unsigned long pause_start_timestamp = 0;
    bool          timer_paused          = false;
};

class Core {
public:
    static bool execute(const ProtocolCommand& command) {
        if (command.realtime) {
            execute_realtime_command(command.rt_cmd, CLIENT_SERIAL);
            return true;
        }

        if (command.gcode == nullptr) {
            return false;
        }

        // system_execute_line() espera un buffer mutable y puede modificar su contenido.
        // Nunca pasar literales en flash casteadas a char* para evitar LoadStoreError.
        char line[192];
        strncpy(line, command.gcode, sizeof(line) - 1);
        line[sizeof(line) - 1] = '\0';

        Error err = system_execute_line(line, (uint8_t)CLIENT_SERIAL, WebUI::AuthenticationLevel::LEVEL_ADMIN);
        return err == Error::Ok;
    }

    static bool start_sd_job(const String& selected_file) {
        char command[192];
        return execute(protocol::run_sd_file(selected_file, command, sizeof(command)));
    }

    static bool safe_stop_job() {
        char temp[50];
        sd_get_current_filename(temp);
        grbl_notifyf("SD print done", "%s print is successful", temp);
        closeFile();
        return get_sd_state(true) == SDState::Idle;
    }

    static bool toggle_pause_resume() {
        if (sys.state == State::Cycle || sys.state == State::Homing || sys.state == State::Jog) {
            return execute(protocol::feed_hold());
        }
        if (sys.state == State::Hold) {
            return execute(protocol::cycle_start());
        }
        return false;
    }

    static bool send_jog(float x, float y, float z, int jog_feed) {
        if (sys.state != State::Idle && sys.state != State::Jog) {
            return false;
        }
        char command[96];
        return execute(protocol::jog(x, y, z, jog_feed, command, sizeof(command)));
    }

    static bool set_zero_all() {
        if (sys.state == State::Alarm) {
            return false;
        }
        client_write(0,"G92 X0 Y0 Z0");
        return true;
    }

    static bool run_home_cycle() {
        if (sys.state == State::Cycle || sys.state == State::Jog) {
            return false;
        }
        return execute(protocol::home_cycle());
    }
};

}  // namespace tft_ui