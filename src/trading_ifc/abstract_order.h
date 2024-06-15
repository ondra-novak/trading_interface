#pragma once

#include "abstract_instrument.h"


#include <memory>
#include <variant>
#include <optional>


namespace trading_ifc {

class Instrument;


class IOrder {
public:

    enum class State {
        ///order is not defined
        undefined,
        ///order object is associated with an instrument, but doesn't represent any real other
        associated,
        ///validation failed
        discarded,
        ///rejected by exchange
        rejected,
        ///order is pending (on exchange)
        pending,
        ///order has been canceled
        canceled,
        ///order has been filled
        filled
    };

    enum class Reason {
        ///no error reported
        no_reason,
        ///discarded because position would be out of limit
        position_limit,
        ///discarded because max leverage would be reached
        max_leverage,
        ///rejected because race condition during replace
        race,
        ///discarded because invalid params
        invalid_params,
        ///discarded because unsuppored
        unsupported,
        ///no funds on exchange
        no_funds,
        ///post only order would cross
        crossing,
        ///error reported on exchange
        exchange_error,
        ///any other internal error
        internal_error
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

    ///Order is undefined
    struct Undefined {};

    ///common part for all orders
    struct Common {
        Side side;
        Behavior behavior;
    };

    ///Market order
    struct Market: Common {
        double amount;
        Market(Side s, double amount, Behavior b = Behavior::standard)
            :Common{s, b},amount(amount) {}
    };

    ///Limit order
    struct Limit: Market {
        double limit_price;
        Limit(Side s, double amount, double limit_price, Behavior b = Behavior::standard)
            :Market(s, amount, b), limit_price(limit_price) {}
    };

    ///Limit post only - rejected if would immediately match
    struct LimitPostOnly: Limit {
        using Limit::Limit;
    };

    ///Stop order
    struct Stop: Market {
        double stop_price;
        Stop(Side s, double amount, double stop_price, Behavior b = Behavior::standard)
            :Market(s, amount, b), stop_price(stop_price) {}
    };

    ///StopLimit order
    struct StopLimit: Stop {
        double limit_price;
        StopLimit(Side s, double amount, double stop_price, double limit_price, Behavior b = Behavior::standard)
            :Stop(s, amount, stop_price, b), limit_price(limit_price) {}
    };

    ///Trailing stop order
    struct TrailingStop: Market {
        double stop_distance;
        TrailingStop(Side s, double amount, double stop_distance, Behavior b = Behavior::standard)
            :Market(s, amount, b), stop_distance(stop_distance) {}
    };

    ///Target and StopLoss order (OCO)
    struct TpSl: StopLimit {
        TpSl(Side s, double amount, double target_price, double stoploss_price, Behavior b = Behavior::standard)
            :StopLimit(s, amount, stoploss_price, target_price, b) {}
    };
    ///Close position order (CFD)
    struct ClosePosition {
        PositionID pos_id;
        ClosePosition(const Account::Position &pos):pos_id(pos.id) {}
    };

    using Setup = std::variant<
                Undefined,
                Market,
                Limit,
                LimitPostOnly,
                Stop,
                StopLimit,
                TrailingStop,
                TpSl,
                ClosePosition
            >;


    constexpr virtual ~IOrder() = default;

    ///get order state
    virtual State get_state() const = 0;

    ///get reason for current state
    virtual Reason get_reason() const = 0;

    ///get message associated with the reason
    virtual std::string get_message() const = 0;

    ///get filled amount
    virtual double get_filled() const = 0;

    ///get last executed price
    virtual double get_last_price() const = 0;

    ///retrieve associated instrument instance
    virtual Instrument get_instrument() const = 0;

    ///retrieve order's initial setup
    virtual Setup get_setup() const = 0;
};

template<typename T>
concept is_order = requires(T order) {
    {order.side}->std::same_as<IOrder::Side>;
    {order.behavior}->std::same_as<IOrder::Behavior>;
};

template<typename T>
concept order_has_amount = (is_order<T> && requires(T order) {
    {order.amount};
});



class NullOrder: public IOrder {
public:
    virtual State get_state() const override {return State::undefined;}
    virtual double get_last_price() const override {return 0.0;}
    virtual std::string get_message() const override {return {};}
    virtual double get_filled() const override {return 0.0;}
    virtual Reason get_reason() const override {return Reason::no_reason;}
    virtual Instrument get_instrument() const override {return {};}
    virtual Setup get_setup() const override {return {};}
    constexpr virtual ~NullOrder() override {}
};

class Order {
public:
    static constexpr NullOrder null_order = {};
    static std::shared_ptr<const IOrder> null_order_ptr;


    Order():_ptr(null_order_ptr) {}
    Order(std::shared_ptr<const IOrder> x): _ptr(std::move(x)) {}

    using State = IOrder::State;
    using Reason = IOrder::Reason;
    using Setup = IOrder::Setup;
    using Market = IOrder::Market;
    using Limit = IOrder::Limit;
    using LimitPostOnly = IOrder::LimitPostOnly;
    using Stop = IOrder::Stop;
    using StopLimit = IOrder::StopLimit;
    using TpSl = IOrder::TpSl;
    using TrailingStop = IOrder::TrailingStop;
    using ClosePosition = IOrder::ClosePosition;
    using Side = IOrder::Side;
    using Behavior = IOrder::Behavior;

    explicit operator bool() const {return _ptr != null_order_ptr;}
    bool defined() const {return _ptr != null_order_ptr;}
    bool operator!() const {return _ptr == null_order_ptr;}

    bool operator==(const Order &other) const = default;

    ///get order state
    State get_state() const {
        return _ptr->get_state();
    }

    ///get reason for current state
    Reason get_reason() const {
        return _ptr->get_reason();
    }

    ///get message associated with the reason
    std::string get_message() const {
        return _ptr->get_message();
    }

    ///get filled amount
    double get_filled() const {
        return _ptr->get_filled();
    }


    ///get total amount
    double get_total() const {
        return std::visit([](const auto &x){
           if constexpr(order_has_amount<decltype(x)>) {
               return x.amount;
           } else {
               return 0;
           }
        }, _ptr->get_setup());
    }

    double get_remain() const {
        return get_total() - get_filled();
    }

    Setup get_setup() const {
        return _ptr->get_setup();
    }

    ///get last executed price
    double get_last_price() const {
        return _ptr->get_last_price();
    }

    Instrument get_instrument() const {
        return _ptr->get_instrument();
    }

    std::optional<Side> get_side() const {
        return std::visit([](const auto &x) ->std::optional<Side> {
            if constexpr(is_order<decltype(x)>) {
                return x.side;
            } else {
                return {};
            }
        }, _ptr->get_setup());
    }

    bool done() const {return get_state()!= Order::State::pending;}
    bool discarded() const {return get_state() == Order::State::discarded;}
    bool rejected() const {return get_state() == Order::State::rejected;}
    bool pending() const {return get_state() == Order::State::pending;}
    bool canceled() const {return get_state() == Order::State::canceled;}

    struct Hasher {
        auto operator()(const Order &ord) const {
            std::hash<std::shared_ptr<const IOrder> > hasher;
            return hasher(ord._ptr);
        }
    };

protected:
    std::shared_ptr<const IOrder> _ptr;

};


inline std::shared_ptr<const IOrder> Order::null_order_ptr = {&Order::null_order, [](auto){}};

}

