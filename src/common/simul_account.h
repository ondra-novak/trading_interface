#pragma once

#include "../trading_ifc/instrument.h"

namespace trading_api {

class SimulAccount: public IAccount {
public:
    SimulAccount();
    virtual ~SimulAccount();
    virtual IAccount::PositionList get_all_positions(
                            const Instrument &instrument) const override;
    virtual IAccount::Position get_position(
                            const Instrument &instrument) const override;
    virtual IAccount::HedgePosition get_hedge_position(
                            const Instrument &instrument) const override;
    virtual IAccount::Info get_info() const override;
};

} /* namespace trading_api */

