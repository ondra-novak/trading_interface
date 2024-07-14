#include "basic_order.h"


namespace trading_api {

AssociatedOrder::AssociatedOrder(Instrument instrument, Account account)
        :_instrument(instrument), _account(account)  {}

AssociatedOrder::State AssociatedOrder::get_state() const {
    return State::associated;
}


Instrument AssociatedOrder::get_instrument() const {
    return _instrument;
}
Account AssociatedOrder::get_account() const {
    return _account;
}

BasicOrder::BasicOrder(Instrument instrument, Account account, Setup setup, Origin origin)
    :_setup(std::move(setup))
    ,_instrument(std::move(instrument))
    ,_account(std::move(account))
    , _origin(origin) {}

BasicOrder::State BasicOrder::get_state() const {
    return _status.last_report.new_state;
}

double BasicOrder::get_last_price() const {
    return _status.last_price;
}

std::string_view BasicOrder::get_message() const {
    return _status.last_report.message;
}

double BasicOrder::get_filled() const {
    return _status.filled;
}

const BasicOrder::Setup &BasicOrder::get_setup() const {
    return _setup;
}

BasicOrder::Reason BasicOrder::get_reason() const {
    return _status.last_report.reason;
}

Instrument BasicOrder::get_instrument() const {
    return _instrument;
}

ErrorOrder::ErrorOrder(Instrument instrument, Account account, Reason r, std::string message)
    :AssociatedOrder(instrument, account)
    ,_r(r)
    ,_message(message) {}

ErrorOrder::State ErrorOrder::get_state() const {
    return State::discarded;
}

ErrorOrder::Reason ErrorOrder::get_reason() const {
    return _r;
}

BasicOrder::Origin BasicOrder::get_origin() const {
    return _origin;
}

std::string_view ErrorOrder::get_message() const {
    return _message;
}

void BasicOrder::Status::add_fill(double price, double amount) {
    last_price = price;
    filled+= amount;
}

void BasicOrder::Status::update_report(Order::Report report) {
    last_report = report;
}

Account BasicOrder::get_account() const {
    return _account;
}

BasicOrder::BasicOrder(Order replaced, Setup setup, bool amend, Origin origin)
    :BasicOrder(replaced.get_instrument(), replaced.get_account(), std::move(setup), std::move(origin)) {
    _replaced = replaced.get_handle();
    _amend = amend;
}

std::string BasicOrder::get_id() const {
    std::ostringstream str;
    str << this;
    return str.str();
}

Order BasicOrder::get_replaced_order() const {
    auto lk = _replaced.lock();
    if (lk) {
        return Order(lk);
    } else {
        return {};
    }
}

}
