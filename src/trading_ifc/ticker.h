#pragma once

#include <chrono>
#include <string>
using Timestamp = std::chrono::system_clock::time_point;


namespace trading_ifc {

struct Ticker {
    Timestamp timestamp;

    //note - if bid and ask are not supported, then bid = ask = last

    double bid;  //current bid price
    double ask;  //current ask price
    double last;    //last execution price - if not known, a price between bid and ask
    double volume;  //execution volume from last update - if unsupported = 0
    double index;   //index price (if known) otherwise zero
};


}
