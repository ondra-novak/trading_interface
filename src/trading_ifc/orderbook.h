#pragma once
#include "account.h"
#include "ticker.h"
#include "wandering_bst.h"
namespace trading_api {


class OrderBook {
public:



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

    void remove_ask_to(double price) {
        while (_ask_side.begin()->first < price) {
            _ask_side.erase(_ask_side.begin()->first);
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
    void update_ticker(Ticker &tk) {
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
    void update_from_ticker(const Ticker &tk) {
        remove_ask_to(tk.ask);
        remove_bid_to(tk.bid);
        update_ask(tk.ask, tk.ask_volume);
        update_bid(tk.bid, tk.bid_volume);
    }

protected:


    struct CmpBid {
        bool operator()(double a, double b) const {return a > b;}
    };
    struct CmpAsk {
        bool operator()(double a, double b) const {return a < b;}
    };

    WanderingTree<double, double, CmpBid> _bid_side;
    WanderingTree<double, double, CmpAsk> _ask_side;
};


}
