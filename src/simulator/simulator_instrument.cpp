#include "simulator_instrument.h"
#include "simulator_account.h"

namespace trading_api {

BaseSimuInstrument::BaseSimuInstrument(const Config &cfg, std::string id)
        :_cfg(cfg), _id(id) {}



const BaseSimuInstrument::Config& BaseSimuInstrument::get_config() const {
    return _cfg;
}

const std::string& BaseSimuInstrument::get_id() const {
    return _id;
}

SimulInstrument::SimulInstrument(
        std::shared_ptr<BaseSimuInstrument> base_instrument, std::shared_ptr<SimulAccount> account, std::string label)
    :_base_instrument(base_instrument)
    ,_account(account)
    ,_label(label) {}

Account SimulInstrument::get_account() const {
    return Account(_account);
}

const SimulInstrument::Config& SimulInstrument::get_config() const {
    return _base_instrument->get_config();
}

std::string SimulInstrument::get_id() const {
    return _base_instrument->get_id();
}

std::string SimulInstrument::get_label() const {
    return _label;
}


double SimulInstrument::get_current_price(Side side) const {
    const TickData &tk = _base_instrument->get_ticker();
    switch (side) {
        case Side::buy: return tk.bid;
        case Side::sell: return tk.ask;
        default: return tk.last;
    }
}

double SimulInstrument::get_current_value(Side side) const {
    const auto &cfg = _base_instrument->get_config();
    return Instrument::quotation_to_price(cfg,get_current_price(side))
            * Instrument::lot_to_amount(cfg, 1);
}

}
