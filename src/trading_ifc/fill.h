#pragma once

#include <string>
#include "timer.h"

#include <vector>

namespace trading_api {


struct Fill {

    ///Fill timestamp (UTC)
    /**
     * Note this field is constant for given fill (with same fill_id).
     * If reported timestamp is different for the same fill_id, the fill
     *  can be reported twice.
     *
     */
    Timestamp time;

    ///Exchange's unique identifier (mandatory)
    /** Unique identifier of this fill. It is used to detect duplicated fills.
     * If this identifier is not unique, fills can be mistakely discarded.
     */
    std::string fill_id;

    ///Exchange's order unique identifier (mandatory)
    /**
     * An identifer of the order, which is responsible of this fill. This
     * id must be filled, but it is allowed to fill the field with
     * information unrelated to real world. However it must be deterministic,
     * for example, when this fill is repeatedly fetched from the exchange,
     * this id must be same for every attempt.
     *
     * Main purpose of such id is help strategy to connect multiple fills to
     * single order - for instance to calculate average fill price
     */
    std::string order_id;

    ///associated instrument identifier
    std::string instrument_id;

    ///Price of this fill
    double price;
    ///Amount exchanged
    /**
     * @note if fees are subtracted from the amount, this field is filled by
     * amount after substraction
     */
    double amount;
    ///Commision (fees)
    /**
     * Absolute amount of fees in instrument's currency. If
     * fees are subsctracted from amount, this value contains fees recalculated
     * to the currency (amount_fees * price)
     */
    double fees;

    bool operator==(const Fill & other) const {
        return fill_id == other.fill_id;
    }

    struct Hasher {
        auto operator()(const Fill &fill) const {
            std::hash<std::string> hasher;
            return hasher(fill.fill_id);
        }
    };

};

using Fills = std::vector<Fill>;


}
