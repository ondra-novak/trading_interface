#pragma once

#include "../../trading_ifc/instrument.h"
#include "../../trading_ifc/micromutex.h"

#include "rpc_client.h"

class BinanceInstrument : public trading_api::IInstrument{
public:

    struct Config: trading_api::Instrument::Config  {
        int quantity_precision;
        int base_asset_precision;
        int quote_precision;
        std::string quote_asset;
        std::string base_asset;
        std::string id;
    };


    BinanceInstrument(std::string_view label,
            const Config &cfg,
            trading_api::Exchange x);

    static const BinanceInstrument &from_instrument(const trading_api::Instrument &i);

    void update_config(RPCClient &client, trading_api::Function<void()> done);


    std::string _label;
    Config _config;
    trading_api::Exchange _x;
    mutable uMutex _mx;

    virtual std::string get_category() const override;
    virtual std::string get_label() const override;
    virtual trading_api::Exchange get_exchange() const override;
    virtual std::string get_id() const override;
    const virtual trading_api::IInstrument::Config& get_config() const override;
};


