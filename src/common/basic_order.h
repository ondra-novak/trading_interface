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
    virtual SerializedOrder to_binary() const {return {};}
    virtual Origin get_origin() const {return Origin::strategy;}

protected:
    Instrument _instrument;
};


class BasicOrder: public IOrder {
public:


    BasicOrder(Setup setup, Instrument instrument, Origin origin);
    virtual State get_state() const override;
    virtual double get_last_price() const override;
    virtual std::string_view get_message() const override;
    virtual double get_filled() const override;
    virtual const Setup &get_setup() const override;
    virtual Reason get_reason() const override;
    virtual Instrument get_instrument() const override;
    virtual SerializedOrder to_binary() const override = 0;
    virtual Origin get_origin() const ;

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


}
