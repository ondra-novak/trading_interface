#pragma once

#include "timer.h"

#include <chrono>
#include <string>


namespace trading_api {

struct Ticker {

    //note - if bid and ask are not supported, then bid = ask = last

    double bid = 0;  //current bid price
    double bid_volume = 0; //current volume on bid
    double ask = 0;  //current ask price
    double ask_volume = 0; //current volume on ask
    double last = 0;    //last execution price - if not known, a price between bid and ask
    double volume = 0;  //execution volume from last update - if unsupported = 0
    double index = 0;   //index price (if known) otherwise zero

};


}
