#pragma once

#include <string>
#include "common.h"
#include "timer.h"
#include "instrument.h"
#include <vector>

namespace trading_api {


///single fill
struct Fill {

    ///Fill timestamp (UTC)
    /**
     * Note this field is constant for given fill (with same fill_id).
     * If reported timestamp is different for the same fill_id, the fill
     *  can be reported twice.
     *
     */
    Timestamp time;

    ///exchange's unique ID
    std::string id;

    ///User defined label (for filtering)
    std::string label;

    ///Position identifier
    /** This is optional id
     * If the string is filled, id contains ID of position, which has been created, increased,
     * reduced or closed. The exchange is responsible to match correct information about positions
     * If the position doesn't exists, or amount of reduced is greater than size of position,
     * extra amount is not counted to overall position.
     * If the string is blank, fill is counted to global position.
     */
    std::string pos_id;

    IInstrument::InstrumentFillInfo instrument;

    ///execution side
    Side side;

    ///Price of this fill
    double price;
    ///Amount exchanged
    /**
     * @note if fees are subtracted from the amount, this field is filled by
     * amount after substraction
     *
     */
    double amount;
    ///Commision (fees)
    /**
     * Absolute amount of fees in instrument's currency. If
     * fees are subsctracted from amount, this value contains fees recalculated
     * to the currency (amount_fees * price)
     *
     */
    double fees;

    bool operator==(const Fill & other) const {
        return id == other.id;
    }

};


using Fills = std::vector<Fill>;

///single position (aggregated from fills)
struct Position {

    ///Position last update time
    Timestamp last_update_time;

    ///Position last_fill_id
    std::string last_fill_id;

    ///User defined label  - retrieves from last trade
    std::string label;

    ///Position identifier
    std::string pos_id;

    ///Information about instrument
    IInstrument::InstrumentFillInfo instrument;

    ///execution side
    /// for position it contains overall side
    Side side;

    ///average open price (ACB)
    double open_price;
    ///Amount exchanged
    /**
     * @note if fees are subtracted from the amount, this field is filled by
     * amount after substraction
     *
     * for position it contains position amount
     */
    double amount;
    ///Commision (fees)
    /**
     * Absolute amount of fees in instrument's currency. If
     * fees are subsctracted from amount, this value contains fees recalculated
     * to the currency (amount_fees * price)
     *
     * for position it contains total fees
     */
    double fees;

    bool operator==(const Position & other) const {
        return pos_id == other.pos_id && instrument == other.instrument;
    }

};


using Positions = std::vector<Position>;

///single closed trade (aggregated from fills)
struct Trade {

    ///Position last update time
    Timestamp last_update_time;

    ///Position last_fill_id
    std::string last_fill_id;

    ///User defined label  - retrieves from last trade
    std::string label;

    ///Information about instrument
    IInstrument::InstrumentFillInfo instrument;

    ///execution side
    /// for position it contains overall side
    Side side;

    ///average open price (ACB)
    double open_price;

    ///closing price
    double close_price;
    ///Amount exchanged
    /**
     * @note if fees are subtracted from the amount, this field is filled by
     * amount after substraction
     *
     * for position it contains position amount
     */
    double amount;
    ///Commision (fees)
    /**
     * Absolute amount of fees in instrument's currency. If
     * fees are subsctracted from amount, this value contains fees recalculated
     * to the currency (amount_fees * price)
     *
     * for position it contains total fees
     */
    double fees;

    double calc_pnl() const {
        if (instrument.type == IInstrument::Type::inverted_contract) {
            return instrument.multiplier * amount * (1.0/open_price - 1.0/close_price);
        } else {
            return instrument.multiplier * amount * (close_price - open_price);
        }
    }

};

using Trades = std::vector<Trade>;

///Aggregated trades for single structure shows profit or loss on single instrument
struct ProfitLoss {

    ///information about instrument
    IInstrument::InstrumentFillInfo instrument;

    ///total pnl
    double pnl;

    ///total fees
    double fees;
};

using TradingStatistics = std::vector<ProfitLoss>;


///calculate statistics from trades
inline TradingStatistics calculate_statistics(const Trades &trades) {
    TradingStatistics stats;
    for (const auto &x: trades) {
        auto f = std::find_if(stats.begin(), stats.end(), [&](const ProfitLoss &pnl){
            return pnl.instrument == x.instrument;
        });
        if (f == stats.end()) {
            stats.push_back({x.instrument, x.calc_pnl(), x.fees});
        } else {
            f->pnl+=x.calc_pnl();
            f->fees+= x.fees;
        }
    }
    return stats;


}


}
