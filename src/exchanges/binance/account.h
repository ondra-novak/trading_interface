#pragma once

#include "identity.h"

#include "../../trading_ifc/account.h"



class BinanceAccount: public trading_api::IAccount {
public:

    using Exchange = trading_api::Exchange;
    using Instrument = trading_api::Instrument;



    BinanceAccount(std::string ident, std::string asset,  Exchange ex, std::string label);

    virtual Info get_info() const override;
    virtual std::string get_label() const override;
    virtual Exchange get_exchange() const override;
    virtual Positions get_positions() const override;


    std::string get_ident() const {return _ident;}

    void update(Info info, Positions positions);

    const std::string &get_asset() const {return _asset;}

protected:
    mutable std::mutex  _mx;
    std::string _ident;
    std::string _asset;
    Exchange _ex;
    std::string _label;
    Info _info;
    Positions _positions;
};