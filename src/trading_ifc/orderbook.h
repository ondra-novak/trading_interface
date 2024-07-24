#pragma once
#include "common.h"
#include "account.h"
#include "tickdata.h"
#include "wandering_bst.h"

namespace trading_api {


class OrderBook {
public:


    struct Update {
        Side side;
        double level;
        double amount;
    };


    auto &bid() {return _bid_side;}
    auto &ask() {return _ask_side;}
    const auto &bid() const {return _bid_side;}
    const auto &ask() const {return _ask_side;}

    void update_bid(double price, double amount) {
        if (amount <= 0) _bid_side.erase(price);
        else _bid_side.replace(price, amount);
    }
    void update_ask(double price, double amount) {
        if (amount <= 0) _ask_side.erase(price);
        else _ask_side.replace(price, amount);
    }

    void update(const Update &up) {
        switch (up.side) {
            case Side::buy: update_bid(up.level, up.amount);break;
            case Side::sell: update_ask(up.level, up.amount);break;
            default:break;
        }
    }

    void remove_ask_to(double price) {
        while (_ask_side.begin()->first < price) {
            _ask_side.erase(_ask_side.begin()->first);
        }
    }

    void trim(double lowest_price, double highest_price) {
        {
            auto iter = _bid_side.upper_bound(lowest_price);
            while (iter != _bid_side.end()) {
                _bid_side.erase(iter->first);
                iter = _bid_side.upper_bound(lowest_price);
            }
        }
        {
            auto iter = _ask_side.upper_bound(highest_price);
            while (iter != _ask_side.end()) {
                _ask_side.erase(iter->first);
                iter = _ask_side.upper_bound(highest_price);
            }
        }
    }

    void remove_bid_to(double price) {
        while (_bid_side.begin()->first > price) {
            _bid_side.erase(_bid_side.begin()->first);
        }
    }

    bool empty() const {
        return _bid_side.empty() && _ask_side.empty();
    }

    ///update ticker's value from orderbook
    void update_ticker(TickData &tk) {
        auto ask_beg = _ask_side.begin();
        auto bid_beg = _bid_side.begin();
        if (ask_beg != _ask_side.end()) {
            tk.ask = ask_beg->first;
            tk.ask_volume = ask_beg->second;
        }
        if (bid_beg != _bid_side.end()) {
            tk.bid = bid_beg->first;
            tk.bid_volume = bid_beg->second;
        }
    }

    ///update orderbook from ticker's values
    /**
     * This simulates orderbook, if only ticker is available
     * @param tk
     */
    void update_from_ticker(const TickData &tk) {
        remove_ask_to(tk.ask);
        remove_bid_to(tk.bid);
        update_ask(tk.ask, tk.ask_volume);
        update_bid(tk.bid, tk.bid_volume);
    }

    void set_timestamp(const Timestamp &tp) {
        _tp = tp;
    }

    Timestamp get_timestamp() const {
        return _tp;
    }



    friend std::ostream &operator<<(std::ostream &s, const OrderBook &ob) {
        auto iter_bid = ob._bid_side.begin();
        auto iter_ask = ob._ask_side.begin();
        double bid_lev[8], bid_amn[8];
        int bid_cnt = 0;
        while (iter_bid != ob._bid_side.end() && bid_cnt < 8) {
            bid_lev[bid_cnt] = iter_bid->first;
            bid_amn[bid_cnt] = iter_bid->second;
            ++bid_cnt;
            ++iter_bid;
        }
        int cnt = bid_cnt;
        while (iter_bid != ob._bid_side.end()) {
            ++iter_bid;
            ++bid_cnt;
        }
        s<<'(' << bid_cnt << ')';
        while (cnt > 0) {
            --cnt;
            s << bid_amn[cnt] << "@" << bid_lev[cnt];
            if (cnt) s << ", ";
        }
        s << "---";
        int ask_cnt = 0;
        while (iter_ask != ob._ask_side.end() && ask_cnt < 8) {
            if (ask_cnt) s<<", ";
            s << iter_ask->second << "@" << iter_ask->first;
            ++ask_cnt;
            ++iter_ask;
        }
        while (iter_ask != ob._ask_side.end()) {
            ++iter_ask;
            ++ask_cnt;
        }
        s << '(' << ask_cnt << ')';
        return s;
    }


protected:


    struct CmpBid {
        bool operator()(double a, double b) const {return a > b;}
    };
    struct CmpAsk {
        bool operator()(double a, double b) const {return a < b;}
    };

    Timestamp _tp = {}; //<snapshot time
    WanderingTree<double, double, CmpBid> _bid_side = {};    //<bid side
    WanderingTree<double, double, CmpAsk> _ask_side = {};    //<ask side
};


}
