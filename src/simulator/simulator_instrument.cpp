#include "simulator_instrument.h"
#include "simulator_account.h"

namespace trading_api {

BaseSimuInstrument::BaseSimuInstrument(const Config &cfg, std::string id)
        :_cfg(cfg), _id(id), _last_price(0) {}


void BaseSimuInstrument::set_last_price(double last_price) {
    _last_price = last_price;
}

double BaseSimuInstrument::get_last_price() const {
    return _last_price;
}

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

double SimulInstrument::get_last_price() const {
    return _base_instrument->get_last_price();
}

}
