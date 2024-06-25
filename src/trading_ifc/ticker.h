#pragma once

#include "timer.h"

#include <chrono>
#include <string>


namespace trading_api {

struct Ticker {
    Timestamp timestamp;

    //note - if bid and ask are not supported, then bid = ask = last

    double bid;  //current bid price
    double bid_volume; //current volume on bid
    double ask;  //current ask price
    double ask_volume; //current volume on ask
    double last;    //last execution price - if not known, a price between bid and ask
    double volume;  //execution volume from last update - if unsupported = 0
    double index;   //index price (if known) otherwise zero
};


}
