#pragma once

#include "../trading_ifc/strategy_context.h"


namespace trading_api {

class SimulInstrument: public IInstrument {
public:



    virtual Account get_account() const override;

    virtual Config get_config() const override;

    virtual std::string get_id() const override;

    double get_last_price() const;

protected:

};

}
