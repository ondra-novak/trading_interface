#pragma once

namespace trading_api {


struct OrderBook {

    struct Level {
        double price;
        double amount;
        unsigned int orders;
    };

    using Side = std::vector<Level>;

    Side ask;
    Side bid;
};


}
