#pragma once

#include "instrument.h"

#include "rest_client.h"
#include <chrono>
#include <vector>
#include <utility>

class InstrumentDefCache {
public:

    bool need_reload() const;
    template<std::invocable<> Fn>
    bool begin_reload(Fn &&fn) {
        std::lock_guard _(_mx);
        _cb_list.push_back(std::forward<Fn>(fn));
        return _pending_counter++ == 0;
    }

    void end_reload() {
        std::vector<trading_api::Function<void()> > tmp;
        {
            std::lock_guard _(_mx);
            if (--_pending_counter > 0) return;
            _pending_counter = 0;
            _expires = std::chrono::system_clock::now() + std::chrono::minutes(1);
            tmp = std::move(_cb_list);
        }
        for (auto &x: tmp) x();
    }

    void reload(RestClient &client, bool coinm);
    std::vector<BinanceInstrument::Config> query(std::string_view query_str) const;
    bool empty() const {return _instruments.empty();}

    auto get_last_error() {
        std::lock_guard _(_mx);
        return std::exchange(_last_error, {});
    }

protected:
    mutable std::mutex _mx;
    std::chrono::system_clock::time_point _expires = {};
    std::vector<BinanceInstrument::Config> _instruments;
    void process(const RestClient::Result &result, bool coinm);
    static bool sort_order(const BinanceInstrument::Config &a, const BinanceInstrument::Config &b);
    std::vector<trading_api::Function<void()> > _cb_list;
    unsigned int _pending_counter = 0;
    json::value_t _last_error;




};

