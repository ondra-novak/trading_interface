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

BasicOrder::BasicOrder(Setup setup, Instrument instrument)
    :_setup(setup),_instrument(instrument)
{

}

BasicOrder::State BasicOrder::get_state() const {
    return _state;
}

double BasicOrder::get_last_price() const {
    return _last_price.load();
}

std::string_view BasicOrder::get_message() const {
    auto ptr = _message.try_get();
    if (ptr) return *ptr;
    else return {};
}

double BasicOrder::get_filled() const {
    return _filled.load();
}

const BasicOrder::Setup &BasicOrder::get_setup() const {
    return _setup;
}

BasicOrder::Reason BasicOrder::get_reason() const {
    return _reason.load();
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
    _message.set(std::move(message));
    _reason = r;
    _state = st;
}

}
