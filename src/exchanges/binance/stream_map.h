#pragma once

#include "ws_streams.h"
#include "../../trading_ifc/log.h"

class StreamMap {
    WSEventListener _lsn;
    std::vector<WSStreams *> _streams;
    std::mutex _mx;
    unsigned int _timeout_interval_sec;
    trading_api::Log _log;
public:
    StreamMap(trading_api::Log log, unsigned int timeout_interval_sec=5);
    ~StreamMap();
    void add_stream(WSStreams *inst);
    void remove_stream(WSStreams *inst);
    bool process_messages(std::chrono::system_clock::time_point stop_tp);
    void signal_exit();
};
