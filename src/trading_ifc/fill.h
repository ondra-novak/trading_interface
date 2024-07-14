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

    ///exchange's unique ID
    std::string id;

    ///User defined label (for filtering)
    std::string label;

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
        return id == other.id;
    }


};

using Fills = std::vector<Fill>;


}
