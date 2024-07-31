#include "stream_map.h"

StreamMap::StreamMap(trading_api::Log log, unsigned int timeout_interval_sec)
    :_timeout_interval_sec(timeout_interval_sec),_log(std::move(log)) {}

StreamMap::~StreamMap() {
    for (auto x: _streams) if (x) x->disable_data_available_notification();
}


void StreamMap::add_stream(WSStreams *inst)
{
    std::lock_guard _(_mx);
    auto iter = std::find(_streams.begin(), _streams.end(), nullptr);
    if (iter == _streams.end()) {
        _streams.push_back(inst);
        inst->notify_data_available(_lsn, _streams.size());
    } else {
        *iter = inst;
        inst->notify_data_available(_lsn, std::distance(_streams.begin(), iter)+1);
    }
}

void StreamMap::remove_stream(WSStreams *inst)
{
    std::lock_guard _(_mx);
    inst->disable_data_available_notification();
    auto iter = std::find(_streams.begin(), _streams.end(), nullptr);
    if (iter != _streams.end()) *iter = nullptr;
}

bool StreamMap::process_messages(std::chrono::system_clock::time_point stop_tp) {
    auto tp = std::chrono::system_clock::now();
    while (tp < stop_tp) {
        auto expr = std::min(stop_tp, tp + std::chrono::seconds(_timeout_interval_sec));
        _lsn.wait_until(expr);
        tp = std::chrono::system_clock::now();
        std::lock_guard _(_mx);
        for (auto x: _lsn) {
            if (x == 0) {
                _log.trace("Signal exit");
                return true;
            }
            --x;
            if (x < _streams.size()) {
                auto ptr = _streams[x];
                if (ptr) ptr->process_responses();
            }
        }
        for (auto ptr: _streams) {
            if (ptr) {
                if (ptr->check_stalled(_timeout_interval_sec)) {
                    _log.warning("Stalled/reconnect: {}", ptr->get_url());
                    ptr->reconnect();
                }
            }
        }
    }
    return false;
}


void StreamMap::signal_exit()
{
    _lsn.signal(0);
}
