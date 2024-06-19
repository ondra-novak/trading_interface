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

protected:
    Instrument _instrument;
};


class BasicOrder: public IOrder {
public:


    BasicOrder(Setup setup, Instrument instrument);
    virtual State get_state() const override;
    virtual double get_last_price() const override;
    virtual std::string_view get_message() const override;
    virtual double get_filled() const override;
    virtual const Setup &get_setup() const override;
    virtual Reason get_reason() const override;
    virtual Instrument get_instrument() const override;

    void add_fill(double price, double amount);
    void set_state(State st);
    void set_state(State st, Reason r, std::string message);



protected:
    Setup _setup;
    std::atomic<double> _filled = 0;
    std::atomic<double> _last_price = 0;
    std::atomic<State> _state = State::pending;
    std::atomic<Reason> _reason = Reason::no_reason;
    Instrument _instrument;
    atomic_future<std::string> _message = {};



};


}
