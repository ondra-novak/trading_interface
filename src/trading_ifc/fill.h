#pragma once

#include <chrono>
#include <string>
using Timestamp = std::chrono::system_clock::time_point;



namespace trading_ifc {


struct Fill {

    Timestamp time;     //time of execution (reported by exchange)
    std::string id;     //exchange's identifier of the fill (optional)
    double price;       //execution price
    double amount;      //if exchange substract fees from amount, this value is adjusted
    double fees;        //fees are recalculated to instrument currency
};


}
