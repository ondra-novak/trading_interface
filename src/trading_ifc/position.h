#pragma once
#include <cstdint>

namespace trading_api {

enum class Side {
    undefined = 0,
    buy = 1,
    sell = -1
};

using PositionID = std::intptr_t;

struct Position {
    PositionID id = {};    //id of position
    Side side = Side::undefined;
    double amount = 0;      //position size (negative is short)
    double open_price = 0;  //open price (if 0 then unknown)
};

}


