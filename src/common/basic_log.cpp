#include "basic_log.h"

namespace trading_api {

void BasicLog::output(Serverity level, std::string_view msg) const {
    std::lock_guard _(_mx);

    auto tp = _time_source?_time_source():std::chrono::system_clock::now();
    std::time_t time_t = std::chrono::system_clock::to_time_t(tp);
    std::tm tm = *std::gmtime(&time_t);
    auto duration = tp.time_since_epoch();
    auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(duration) % 1000;

    // Format the time to RFC3339 string
    _f << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
    _f << '.' << std::setw(3) << std::setfill('0') << millis.count();
    _f << " (" << to_string(level) << ") " << msg << "\n";

}

void BasicLog::set_time_source(TimeSource tmsrc) {
    std::lock_guard _(_mx);
    _time_source = std::move(tmsrc);


}

}
