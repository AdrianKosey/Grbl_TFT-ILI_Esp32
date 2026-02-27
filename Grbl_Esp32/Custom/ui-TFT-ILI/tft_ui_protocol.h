#pragma once

#include <Arduino.h>

namespace tft_ui {

struct ProtocolCommand {
    const char* gcode;
    bool        realtime;
    Cmd         rt_cmd;
};

namespace protocol {
inline ProtocolCommand feed_hold() {
    return { nullptr, true, Cmd::FeedHold };
}

inline ProtocolCommand cycle_start() {
    return { nullptr, true, Cmd::CycleStart };
}

inline ProtocolCommand run_sd_file(const String& path, char* out, size_t out_size) {
    snprintf(out, out_size, "$SD/Run=%s", path.c_str());
    return { out, false, Cmd::StatusReport };
}

inline ProtocolCommand set_work_zero_all() {
    return { "G10 L20 P1 X0 Y0 Z0", false, Cmd::StatusReport };
}

inline ProtocolCommand home_cycle() {
    return { "$H", false, Cmd::StatusReport };
}

inline ProtocolCommand go_work_zero_xy() {
    return { "G90 G0 X0 Y0", false, Cmd::StatusReport };
}

inline ProtocolCommand jog(float x, float y, float z, int feed, char* out, size_t out_size) {
    snprintf(out, out_size, "$J=G91 G21 X%.3f Y%.3f Z%.3f F%d", x, y, z, feed);
    return { out, false, Cmd::StatusReport };
}

inline ProtocolCommand stop_spindle() {
    return { "M5", false, Cmd::StatusReport };
}

}  // namespace protocol
}  // namespace tft_ui