#pragma once

#include "simulator_account.h"
#include "../trading_ifc/strategy_context.h"
#include "../trading_ifc/orderbook.h"


namespace trading_api {

class SimulAccount;

class BaseSimuInstrument {
public:

    using Config = Instrument::Config;

    BaseSimuInstrument(const Config &cfg, std::string id);

    const Config &get_config() const;
    const std::string &get_id() const;

    const OrderBook& get_orderbook() const {
        return _orderbook;
    }

    void set_orderbook(const OrderBook &orderbook) {
        _orderbook = orderbook;
        _orderbook.update_ticker(_ticker);
    }

    void set_last_price(double price) {
        _ticker.last = price;
    }

    const TickData& get_ticker() const {
        return _ticker;
    }

    void set_ticker(const TickData &ticker) {
        _ticker = ticker;
        _orderbook.update_from_ticker(ticker);
    }

protected:
    Config _cfg;
    std::string _id;
    TickData _ticker;
    OrderBook _orderbook;

};

class SimulInstrument: public IInstrument {
public:

    SimulInstrument(std::shared_ptr<BaseSimuInstrument> base_instrument,
            std::shared_ptr<SimulAccount> account, std::string label);

    virtual Account get_account() const override;

    virtual const Config &get_config() const override;

    virtual std::string get_id() const override;
    virtual std::string get_label() const override;

    ///gets current quotation price
    /**
     * @param side specify buy for bid and sell for ask
     * @return price
     */
    double get_current_price(Side side) const ;
    ///gets current value of instrument for 1 share
    /**
     * @param side specify buy for bid and sell for ask
     * @return price
     */
    double get_current_value(Side side) const ;

    SimulAccount *get_simul_account() const {
        return _account.get();
    }


protected:
    std::shared_ptr<BaseSimuInstrument> _base_instrument;
    std::shared_ptr<SimulAccount> _account;
    std::string _label;

};


}
