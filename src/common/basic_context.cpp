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

}
