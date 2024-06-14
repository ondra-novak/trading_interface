#pragma once

#include "abstract_instrument.h"
#include "order_params.h"

#include <memory>


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

    constexpr virtual ~IOrder() = default;

    ///get order state
    virtual State get_state() const = 0;

    ///get reason for current state
    virtual Reason get_reason() const = 0;

    ///get message associated with the reason
    virtual std::string get_message() const = 0;

    ///get filled amount
    virtual double get_filled() const = 0;

    ///get total amount
    virtual double get_total() const = 0;

    ///get last executed price
    virtual double get_last_price() const = 0;

    ///retrieve associated instrument instance
    virtual Instrument get_instrument() const = 0;
};


class NullOrder: public IOrder {
public:
    virtual State get_state() const override {return State::undefined;}
    virtual double get_last_price() const override {return 0.0;}
    virtual std::string get_message() const override {return {};}
    virtual double get_filled() const override {return 0.0;}
    virtual Reason get_reason() const override {return Reason::no_reason;}
    virtual Instrument get_instrument() const override {return {};}
    virtual double get_total() const override {return 0.0;}
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
        return _ptr->get_total();
    }

    ///get last executed price
    double get_last_price() const {
        return _ptr->get_last_price();
    }

    Instrument get_instrument() const {
        return _ptr->get_instrument();
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

