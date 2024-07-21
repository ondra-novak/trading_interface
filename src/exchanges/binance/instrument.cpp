#include "instrument.h"

BinanceInstrument::BinanceInstrument(std::string label,
        const Config &cfg,
        trading_api::Exchange x)
:_label(std::move(label))
,_config(cfg)
,_x(std::move(x))
{

}

const BinanceInstrument& BinanceInstrument::from_instrument(const trading_api::Instrument &i) {
    return dynamic_cast<const BinanceInstrument &>(*i.get_handle());
}

void BinanceInstrument::update_config(RPCClient &client,trading_api::Function<void()> done) {

}

std::string BinanceInstrument::get_category() const {return {};}


std::string BinanceInstrument::get_label() const {
    return _label;
}

trading_api::Exchange BinanceInstrument::get_exchange() const {
    return _x;
}

std::string BinanceInstrument::get_id() const {
    return _config.id;
}

const trading_api::IInstrument::Config& BinanceInstrument::get_config() const {
    std::lock_guard _(_mx);
    return _config;
}
