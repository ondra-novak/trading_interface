#pragma once

#include <mutex>
#include "../trading_ifc/strategy_context.h"

#include "simulator_instrument.h"

namespace trading_api {

class SimulAccount: public IAccount {
public:

    SimulAccount(std::string label, std::string currency, double equity, double leverage);
    //frontent
    virtual PositionList get_all_positions(const Instrument &instrument) const override;
    virtual OverallPosition get_position(const Instrument &instrument) const override;
    virtual Position get_position_by_id(const Instrument &instrument,PositionID id) const override;
    virtual std::string get_label() const override;
    virtual HedgePosition get_hedge_position(const Instrument &instrument) const override;
    virtual Info get_info() const override;

    //backend

    double record_fill(const Instrument &instrument, Side side, double price, double amount, Order::Behavior behaviour);
    bool close_position(const Instrument &instrument, PositionID pos, double price);

    double calc_blocked() const;

    static constexpr PositionID overall_position = -1;
    static constexpr PositionID buy_position = -2;
    static constexpr PositionID sell_position = -3;

protected:

    struct InstrumentInfo {
        std::weak_ptr<IInstrument> instrument;
        PositionList list;
        OverallPosition overall;
    };

    mutable std::mutex _mx;

    std::string _label;
    std::string _currency;
    double _equity;
    double _leverage;
    double _fees = 0;
    PositionID _position_counter = 1;

    mutable std::unordered_map<const IInstrument *, InstrumentInfo> _positions;
    void realize_position(const Instrument::Config &icfg, const Position &pos, double price);

    static OverallPosition calc_overall(const InstrumentInfo &ii);
    static void update_overall(InstrumentInfo &ii) ;

};

}
