
#include "simul_account.h"

namespace trading_api {

SimulAccount::SimulAccount() {
    // TODO Auto-generated constructor stub

}

SimulAccount::~SimulAccount() {
    // TODO Auto-generated destructor stub
}

trading_api::IAccount::PositionList SimulAccount::get_all_positions(
        const Instrument &instrument) const {
}

trading_api::IAccount::Position SimulAccount::get_position(
        const Instrument &instrument) const {
}

trading_api::IAccount::HedgePosition SimulAccount::get_hedge_position(
        const Instrument &instrument) const {
}

trading_api::IAccount::Info SimulAccount::get_info() const {
}

} /* namespace trading_api */
