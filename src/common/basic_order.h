#pragma once


#include "atomic_future.h"
#include "../trading_ifc/order.h"
#include "../trading_ifc/instrument.h"

namespace trading_api {

class AssociatedOrder: public NullOrder {
public:

    AssociatedOrder(Instrument instrument);

    virtual Instrument get_instrument() const override;
    virtual State get_state() const override;
    virtual SerializedOrder to_binary() const override {return {};}
    virtual Origin get_origin() const override {return Origin::strategy;}

protected:
    Instrument _instrument;
};

class ErrorOrder: public AssociatedOrder {
public:
    ErrorOrder(Instrument instrument, Reason r, std::string message);

    virtual State get_state() const override;
    virtual Reason get_reason() const override;
    virtual std::string_view get_message() const override;
    virtual Origin get_origin() const override {return Origin::strategy;}
protected:
    Reason _r;
    std::string _message;
};

class BasicOrder: public IOrder {
public:


    BasicOrder(Instrument instrument, Setup setup, Origin origin);
    virtual State get_state() const override;
    virtual double get_last_price() const override;
    virtual std::string_view get_message() const override;
    virtual double get_filled() const override;
    virtual const Setup &get_setup() const override;
    virtual Reason get_reason() const override;
    virtual Instrument get_instrument() const override;
    virtual SerializedOrder to_binary() const override = 0;
    virtual Origin get_origin() const override ;

    void add_fill(double price, double amount);
    void set_state(State st);
    void set_state(State st, Reason r, std::string message);



protected:
    Setup _setup;
    Instrument _instrument;
    Origin _origin;

    double _filled = 0;
    double _last_price = 0;
    State _state = State::sent;
    Reason _reason = Reason::no_reason;
    std::string _message = {};
};

using PBasicOrder = std::shared_ptr<BasicOrder>;
using PCBasicOrder = std::shared_ptr<const BasicOrder>;


}
