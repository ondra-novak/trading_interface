#include "account.h"
#include "../../common/acb.h"



BinanceAccount::BinanceAccount(PIdentity ident, Exchange ex, std::string label)
    :_ident(std::move(ident))
    , _ex(std::move(ex))
    ,_label(std::move(label))
{
}

BinanceAccount::Info BinanceAccount::get_info() const {
    return _info;
}


std::string BinanceAccount::get_label() const {
    return _label;
}


BinanceAccount::Exchange BinanceAccount::get_exchange() const {
}
