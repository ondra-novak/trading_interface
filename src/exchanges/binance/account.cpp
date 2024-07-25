#include "account.h"
#include "../../common/acb.h"



BinanceAccount::BinanceAccount(std::string ident, std::string asset, Exchange ex, std::string label)
    :_ident(std::move(ident))
    ,_asset(std::move(asset))
    , _ex(std::move(ex))
    ,_label(std::move(label))
{
}

BinanceAccount::Info BinanceAccount::get_info() const {
    std::lock_guard _(_mx);
    return _info;
}


std::string BinanceAccount::get_label() const {
    return _label;
}

BinanceAccount::Positions BinanceAccount::get_positions(const Instrument &i) const {
    std::lock_guard _(_mx);
    auto iter = _positions.find(i);
    if (iter != _positions.end()) return iter->second;
    return {};
}

BinanceAccount::Exchange BinanceAccount::get_exchange() const {
    return _ex;
}

void BinanceAccount::update(Info info, PositionMap positions) {
    std::lock_guard _(_mx);
    _positions = std::move(positions);
    _info = std::move(info);
}
