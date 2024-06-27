#pragma once

#include "simulator_account.h"
#include "../trading_ifc/strategy_context.h"


namespace trading_api {

class SimulAccount;

class BaseSimuInstrument {
public:

    using Config = Instrument::Config;

    BaseSimuInstrument(const Config &cfg, std::string id);

    void set_last_price(double last_price);
    double get_last_price() const ;
    const Config &get_config() const;
    const std::string &get_id() const;
protected:
    Config _cfg;
    std::string _id;
    double _last_price;

};

class SimulInstrument: public IInstrument {
public:

    SimulInstrument(std::shared_ptr<BaseSimuInstrument> base_instrument,
            std::shared_ptr<SimulAccount> account, std::string label);

    virtual Account get_account() const override;

    virtual const Config &get_config() const override;

    virtual std::string get_id() const override;
    virtual std::string get_label() const override;

    double get_last_price() const ;

    SimulAccount *get_simul_account() const {
        return _account.get();
    }


protected:
    std::shared_ptr<BaseSimuInstrument> _base_instrument;
    std::shared_ptr<SimulAccount> _account;
    std::string _label;

};


}
