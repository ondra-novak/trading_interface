#include "basic_context.h"

namespace trading_api {

void BasicContext::on_event(Order ord,
        Fill fill) {
}

void BasicContext::on_event(Order ord) {
}

void BasicContext::on_event(Instrument i, const OrderBook &ord) {
}

void BasicContext::on_event(Instrument i) {
}

void BasicContext::on_event(Instrument i, const Ticker &tk) {
}

void BasicContext::on_event(Account a) {
}

void BasicContext::subscribe(SubscriptionType type,
        const Instrument &i, TimeSpan interval) {
}

Order BasicContext::replace(const Order &order,
        const Order::Setup &setup, bool amend) {
}

Value BasicContext::get_param(std::string_view varname) const {
}

Fills BasicContext::get_fills(std::size_t limit) {
}

Order BasicContext::place(
        const Instrument &instrument,
        const Order::Setup &setup) {
}

void BasicContext::cancel(const Order &order) {
}

TimerID BasicContext::set_timer(Timestamp at,
        CompletionCB fnptr) {
}

void BasicContext::unsubscribe(SubscriptionType type,
        const Instrument &i) {
}

Timestamp BasicContext::now() const {
}

Order BasicContext::create(
        const Instrument &instrument) {
}

void BasicContext::update_account(const Account &a,
        CompletionCB complete_ptr) {
}

void BasicContext::allocate(const Account &a, double equity) {
}

bool BasicContext::clear_timer(TimerID id) {
}

Value BasicContext::get_var(int idx) const {
}

void BasicContext::update_instrument(const Instrument &i,
        CompletionCB complete_ptr) {
}

void BasicContext::set_var(int idx, const Value &val) {
}

void BasicContext::reschedule(Timestamp tp) {

}

}
