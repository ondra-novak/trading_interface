#pragma once
#include "account.h"

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

    bool empty() const {
        return _bid_side.empty() && _ask_side.empty();
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
