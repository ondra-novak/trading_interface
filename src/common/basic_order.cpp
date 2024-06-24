#include "basic_order.h"


namespace trading_api {

AssociatedOrder::AssociatedOrder(Instrument instrument)
        :_instrument(instrument)
{

}

AssociatedOrder::State AssociatedOrder::get_state() const {
    return State::associated;
}


Instrument AssociatedOrder::get_instrument() const {
    return _instrument;
}

BasicOrder::BasicOrder(Setup setup, Instrument instrument, Origin origin)
    :_setup(setup),_instrument(instrument), _origin(origin)
{

}

BasicOrder::State BasicOrder::get_state() const {
    return _state;
}

double BasicOrder::get_last_price() const {
    return _last_price;
}

std::string_view BasicOrder::get_message() const {
    return _message;
}

double BasicOrder::get_filled() const {
    return _filled;
}

const BasicOrder::Setup &BasicOrder::get_setup() const {
    return _setup;
}

BasicOrder::Reason BasicOrder::get_reason() const {
    return _reason;
}

Instrument BasicOrder::get_instrument() const {
    return _instrument;
}

void BasicOrder::add_fill(double price, double amount) {
    _last_price = price;
    _filled += amount;
}

void BasicOrder::set_state(State st) {
    _state = st;
}

void BasicOrder::set_state(State st, Reason r, std::string message) {
    _message = std::move(message);
    _reason = r;
    _state = st;
}

}
