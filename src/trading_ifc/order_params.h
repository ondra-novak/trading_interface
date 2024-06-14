#pragma once



namespace trading_ifc {


struct OrderParams {

    enum class Type {
        ///market order (just amount)
        market,
        ///limit order (amount, limit_price
        limit,
        ///limit order post only (amount, limit_price)
        post,
        ///stop order (amount, stop_price)
        stop,
        ///stop limit order (amount, stop_price, limit_price)
        stop_limit,
        ///trailing stop order (amount, trailing_distance)
        trailing_stop,
        ///pair of orders for target profit and stop loss
        tp_sl,
        ///not exactly order - only triggers when price is reached
        alert,
        ///close given position (CFD), you identify position by position_id
        close
    };

    enum class Side {
        ///buy side
        buy,
        ///sell side
        sell
    };

    enum class Behavior {
        ///standard behavior, reduce position, then open new position (no hedge)
        standard,
        ///increase position on given side (so we can have both buy and sell sides opened)
        hedge,
        ///reduce position, prevent to go other side (you need to SELL on long, BUY on short)
        reduce
    };


    ///order type
    Type type;
    ///order side
    Side side;
    ///amount
    double amount;      //amount (note, can be adjusted to lot size)
    union {
        double limit_price; //limit price (note, can be adjusted to tick size)
        double target_price;
        long position_id;  //uid of position to close
    };
    union {
        double stop_price;  //stop price
        double trailing_distance;
        double stop_loss_price;
    };
    ///behavior how position is changed
    Behavior behavior = Behavior::standard;
    ///set leverage, use zero to request shared leverage
    double leverage = 0;
};


}
