#pragma once

#include "identity.h"

#include "../../trading_ifc/account.h"



class BinanceAccount: public trading_api::IAccount {
public:

    using Exchange = trading_api::Exchange;
    using Instrument = trading_api::Instrument;



    BinanceAccount(PIdentity ident, Exchange ex, std::string label);

    virtual Info get_info() const override;
    virtual std::string get_label() const override;
    virtual Exchange get_exchange() const override;




protected:
    mutable std::mutex  _mx;
    PIdentity _ident;
    std::string _label;
    Exchange _ex;
    Info _info;
};
