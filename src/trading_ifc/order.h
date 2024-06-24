#pragma once

#include "instrument.h"

#include <memory>
#include <variant>
#include <optional>


namespace trading_api {

class Instrument;

using SerializedOrder = std::string;

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
        ///order is being sent to the exchange
        sent,
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
        ///rejected during replace because there is unprocessed fill on way
        replace_unprocessed_fill,
        ///discarded because invalid params
        invalid_params,
        ///discarded because invalid usage of amend
        invalid_amend,
        ///discarded because unsuppored by service provider
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


    enum class Behavior {
        ///standard behavior, reduce position, then open new position (no hedge)
        standard,
        ///increase position on given side (so we can have both buy and sell sides opened)
        hedge,
        ///reduce position, prevent to go other side (you need to SELL on long, BUY on short)
        reduce
    };

    enum class Origin {
        ///Unknown origin (there is no evidence who is responsible for creation of this order)
        unknown,
        ///Order has been created by this strategy instance
        strategy,
        ///Order has been restored from permanent storage
        restored,
        ///Order is liquidation order issued by exchange
        liquidation,
        ///Order is probably made manually by user intervention
        manual,
    };

    ///Order is undefined
    struct Undefined {};

    struct Options {
        ///specifies matching behavior
        Behavior behavior = Behavior::standard;
        ///specifies new position leverage - if applicable - default value: use shared leverage
        double leverage = 0;
        ///specified amount is in volume - so amount of money you want to spend on retrieve on trade (fees are not included)
        /** especially market orders can be executed as series of IOC orders if the
         * exchange doesn't support this feature
         */
        bool amount_is_volume = false;

        ///Specifies constrain on filled amount when order is being replaced
        /**
         *
         * Creates a constrain which is active during replace. This constrain is tested
         * when cancel+place is used to implement replace (amend == false). The
         * service provider checks the filled field on canceled order and if this number
         * is below or equal to specified value, the new order is placed. If this
         * number is above specified value, the new order is rejected.
         *
         * In all cases, the original order is canceled.
         *
         * This constrain should be checked atomically. If this not handled by exchange,
         * the service provider must handle it by self. In this case, there can
         * be a small lag between cancel and place, when the service provider must check
         * the filled amount. If this field is left on default value, the constrain is not
         * checked.
         *
         * This option is not checked when new order is created using place().
         */
        double replace_filled_constrain = std::numeric_limits<double>::max();

        static constexpr Options Default() {return {};}
    };


    ///common part for all orders
    struct Common {
        Side side = {};
        Options options = {};
    };

    ///Market order
    struct Market: Common {
        double amount;
        Market(Side s, double amount, Options opt = Options::Default())
            :Common{s, opt},amount(amount) {}
    };

    ///Limit order
    struct Limit: Market {
        double limit_price;
        Limit(Side s, double amount, double limit_price, Options opt = Options::Default())
            :Market(s, amount, opt), limit_price(limit_price) {}
    };

    ///Limit post only - rejected if would immediately match
    struct LimitPostOnly: Limit {
        using Limit::Limit;
    };

    ///Limit immediate or cancel - rejects when order ends in orderbook (just fill up to limit)
    struct ImmediateOrCancel: Limit {
        using Limit::Limit;

    };

    ///Stop order
    struct Stop: Market {
        double stop_price;
        Stop(Side s, double amount, double stop_price, Options opt = Options::Default())
            :Market(s, amount, opt), stop_price(stop_price) {}
    };

    ///StopLimit order
    struct StopLimit: Stop {
        double limit_price;
        StopLimit(Side s, double amount, double stop_price, double limit_price, Options opt = Options::Default())
            :Stop(s, amount, stop_price, opt), limit_price(limit_price) {}
    };

    ///Trailing stop order
    struct TrailingStop: Market {
        double stop_distance;
        TrailingStop(Side s, double amount, double stop_distance, Options opt = Options::Default())
            :Market(s, amount, opt), stop_distance(stop_distance) {}
    };

    ///Target and StopLoss order (OCO)
    struct TpSl: StopLimit {
        TpSl(Side s, double amount, double target_price, double stoploss_price, Options opt = Options::Default())
            :StopLimit(s, amount, stoploss_price, target_price, opt) {}
    };
    ///Close position order (CFD)
    struct ClosePosition {
        PositionID pos_id;
        ClosePosition(const Account::Position &pos):pos_id(pos.id) {}
    };

    ///Transfer money from one account to other account
    /**
     *  This order can be executed on any instrument (as instrument is ignored), however
     *  it is strongly recommended, to use instrument which is associated with the
     *  source account
     *
     *  The order doesn't generate fill. However upon successfull execution, it
     *  generates on_order, which is changed to filled
     *
     *  If transfer requires currency exchange, it is executed as spot market order on
     *  available pair. Such exchange is made only between known currency pairs, no
     *  transitional execution is performed
     */
    struct Transfer {
        Account source;
        Account target;
        double amount;
        Transfer(const Account &source, const Account &target, double amount)
            :source(source), target(target), amount(amount) {}
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
                ClosePosition,
                Transfer
            >;


    constexpr virtual ~IOrder() = default;

    ///get order state
    virtual State get_state() const = 0;

    ///get reason for current state
    virtual Reason get_reason() const = 0;

    ///get message associated with the reason
    virtual std::string_view get_message() const = 0;

    ///get filled amount
    virtual double get_filled() const = 0;

    ///get last executed price
    virtual double get_last_price() const = 0;

    ///retrieve associated instrument instance
    virtual Instrument get_instrument() const = 0;

    ///retrieve order's initial setup
    virtual const Setup &get_setup() const = 0;

    ///retrieve binary representation
    virtual SerializedOrder to_binary() const = 0;

    virtual Origin get_origin() const = 0;
};

template<typename T>
concept is_order = requires(T order) {
    {order.side}->std::same_as<Side>;
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
    virtual std::string_view get_message() const override {return {};}
    virtual double get_filled() const override {return 0.0;}
    virtual Reason get_reason() const override {return Reason::no_reason;}
    virtual Instrument get_instrument() const override {return {};}
    virtual SerializedOrder to_binary() const override {return {};}
    virtual Origin get_origin() const override {return Origin::unknown;};
    virtual const Setup &get_setup() const override {
        static Setup empty;
        return empty;
    }
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
    using Transfer = IOrder::Transfer;
    using Behavior = IOrder::Behavior;
    using Options = IOrder::Options;
    using Origin = IOrder::Origin;

    explicit operator bool() const {return _ptr != null_order_ptr;}
    bool defined() const {return _ptr != null_order_ptr;}
    bool operator!() const {return _ptr == null_order_ptr;}

    bool operator==(const Order &other) const = default;
    std::strong_ordering operator<=>(const Order &other) const = default;

    ///get order state
    State get_state() const {
        return _ptr->get_state();
    }

    ///get reason for current state
    Reason get_reason() const {
        return _ptr->get_reason();
    }

    ///get message associated with the reason
    std::string_view get_message() const {
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

    ///remain amount to fill
    double get_remain() const {
        return get_total() - get_filled();
    }

    ///order's initial setup
    const Setup &get_setup() const {
        return _ptr->get_setup();
    }

    ///get last executed price
    double get_last_price() const {
        return _ptr->get_last_price();
    }

    ///associated instrument
    Instrument get_instrument() const {
        return _ptr->get_instrument();
    }

    ///order's side
    std::optional<Side> get_side() const {
        return std::visit([](const auto &x) ->std::optional<Side> {
            if constexpr(is_order<decltype(x)>) {
                return x.side;
            } else {
                return {};
            }
        }, _ptr->get_setup());
    }

    ///returns true, if order is finished
    bool done() const {return get_state()!= Order::State::pending;}
    ///returns true, if order is discard
    bool discarded() const {return get_state() == Order::State::discarded;}
    ///returns true, if order is rejected
    bool rejected() const {return get_state() == Order::State::rejected;}
    ///returns true, if order is pending
    bool pending() const {return get_state() == Order::State::pending;}
    ///returns true, if order is canceled
    bool canceled() const {return get_state() == Order::State::canceled;}

    ///Retrieve serialized binary content of the order
    /**
     * @return a binary representation of the order in format specific to
     * service provider. The returned information can be used to
     * store order permanently (for example in database) and use this
     * information to restore the state of the order when connection
     * to the exchange is reestablished.
     *
     * The strategy retrieves state of all stored orders after init() through
     * on_order or on_fill
     */
    SerializedOrder to_binary() const {
        return _ptr->to_binary();
    }

    ///Retrieves order's origin (who created this order)
    /**
     * The strategy can receive orders that was not created by them. This
     * function returns information about who is responsible for creation
     * of this order.
     *
     * For example if the unexcpected order has origin `restored` the strategy
     * knows, that this order was created by previous instance, so it
     * can adapt its state and include this order to calculations
     *
     * @return order's origin
     */
    Origin get_origin() const {
        return _ptr->get_origin();
    }

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

