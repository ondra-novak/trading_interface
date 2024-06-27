#include "simulator_account.h"

#include <algorithm>
#include "../common/acb.h"

namespace trading_api {

SimulAccount::SimulAccount(std::string label, std::string currency, double equity, double leverage)
        :_label(label), _currency(currency), _equity(equity), _leverage(leverage)
{

}

double SimulAccount::record_fill(const Instrument &instrument, Side side, double price,
        double amount, Order::Behavior behaviour) {

    std::lock_guard _(_mx);
    const auto &icfg = instrument.get_config();


    InstrumentInfo &ii = _positions[instrument.get_handle().get()];
    ii.update_overall = true;
    if (behaviour != Order::Behavior::hedge) {
        Side close_side = reverse(side);
        ii.list.erase(std::remove_if(ii.list.begin(), ii.list.end(), [&](const Position &pos){
            if (pos.side == close_side && pos.amount <= amount+pos.amount*1e-10) {
                realize_position(icfg, pos, price);
                amount -= pos.amount;
                return true;
            }
            return false;
        }), ii.list.end());
        if (amount > 0) {
            auto iter = std::find_if(ii.list.begin(), ii.list.end(), [&](const Position &pos){
                return pos.side == close_side;
            });
            if (iter != ii.list.end()) {
                Position p = *iter;
                iter->id = _position_counter++;
                iter->amount -= amount;
                realize_position(icfg, p, price);
                return 0;
            }
        }
        if (behaviour == Order::Behavior::reduce) {
            return 0;
        }
    }
    if (amount > 0) {
        double blk = calc_blocked();
        double lev = _leverage?_leverage:1.0;
        blk += Instrument::calc_margin(icfg, price, amount, lev);
        if (blk > _equity) {
            return amount;
        }
        ii.list.push_back({
            _position_counter++,
            side,
            amount,
            price,
        });
    }
    return 0;

}

SimulAccount::PositionList SimulAccount::get_all_positions(const Instrument &instrument) const {
    std::lock_guard _(_mx);
    auto iter = _positions.find(instrument.get_handle().get());
    if (iter == _positions.end()) return {};
    return iter->second.list;
}

SimulAccount::OverallPosition SimulAccount::get_position(const Instrument &instrument) const {
    std::lock_guard _(_mx);
    auto iter = _positions.find(instrument.get_handle().get());
    if (iter == _positions.end()) return {};
    if (iter->second.update_overall) update_overall(iter->second);
    return iter->second.overall;

}

double SimulAccount::calc_blocked() const {
    double blocked = 0;
    double maintenace_leverage = _leverage?_leverage/2.0:1;
    for (auto &[k, v]: _positions) {
        if (v.update_overall) {
            update_overall(v);
        }
        auto instrument = v.instrument.lock();
        auto siminstr = dynamic_cast<const SimulInstrument *>(instrument.get());
        if (siminstr == nullptr) continue;

        double last_price = siminstr->get_last_price();
        if (last_price <= 0) last_price = v.overall.open_price;

        const auto &cfg = instrument->get_config();
        blocked += v.overall.locked_in_pnl;
        blocked += Instrument::calculate_pnl(cfg, v.overall, last_price);
        blocked += Instrument::calc_margin(cfg, v.overall.amount,  v.overall.open_price , maintenace_leverage);

    }
    return blocked;
}

void SimulAccount::update_overall(InstrumentInfo &ii)  {
    ii.overall = calc_overall(ii);
    ii.update_overall = false;
}

SimulAccount::OverallPosition SimulAccount::calc_overall(const InstrumentInfo &ii) {
    OverallPosition out;
    out.id = overall_position;
    ACB acb;

    for (const Position &pos: ii.list) {
        acb = acb(pos.open_price, pos.amount * static_cast<int>(pos.side));
    }

    double a = acb.getPos();
    out.amount = std::abs(a);
    out.locked_in_pnl = acb.getRPnL();
    out.open_price = acb.getOpen();
    out.side = a < 0?Side::sell:a>0?Side::buy:Side::undefined;
    return out;

}

SimulAccount::HedgePosition SimulAccount::get_hedge_position(const Instrument &instrument) const {
    HedgePosition hg;
    hg.buy.id = buy_position;
    hg.sell.id = sell_position;
    ACB buy_acb;
    ACB sell_acb;
    std::lock_guard _(_mx);
    auto iter = _positions.find(instrument.get_handle().get());
    if (iter != _positions.end()) {
        for (const Position &pos: iter->second.list) {
            switch (pos.side) {
                case Side::buy: buy_acb = buy_acb(pos.open_price, pos.amount); break;
                case Side::sell: sell_acb = sell_acb(pos.open_price, -pos.amount); break;
                default: break;
            }
        }
    }
    hg.buy.amount = buy_acb.getPos();
    hg.buy.open_price= buy_acb.getOpen();
    hg.buy.side = Side::buy;
    hg.sell.amount = -sell_acb.getPos();
    hg.sell.open_price= sell_acb.getOpen();
    hg.sell.side = Side::sell;
    return hg;
}

SimulAccount::Position SimulAccount::get_position_by_id(const Instrument &instrument, PositionID id) const {
    switch (id) {
        case overall_position:return get_position(instrument);
        case buy_position: return get_hedge_position(instrument).buy;
        case sell_position: return get_hedge_position(instrument).sell;
        default: {
            std::lock_guard _(_mx);
            auto iter = _positions.find(instrument.get_handle().get());
            if (iter != _positions.end()) {
                const PositionList &lst = iter->second.list;
                auto iter2 = std::find_if(lst.begin(), lst.end(), [&](const Position &pos){
                    return pos.id == id;
                });
                if (iter2 != lst.end()) return *iter2;
            }
            return {};
        }
    }
}

std::string SimulAccount::get_label() const {
    return _label;
}

SimulAccount::Info SimulAccount::get_info() const {
    double blocked = calc_blocked();
    double balance = _equity * _leverage - blocked;
    return {
        _equity,
         balance,
         blocked,
        _leverage,
        _currency

    };
}


void SimulAccount::realize_position(const Instrument::Config &icfg, const Position &pos, double price) {

    _equity += Instrument::calculate_pnl(icfg, pos, price);

}

bool SimulAccount::close_position(const Instrument &instrument, PositionID id, double price) {
    std::lock_guard _(_mx);
    InstrumentInfo &ii = _positions[instrument.get_handle().get()];
    if (ii.list.empty()) return false;
    ii.update_overall = true;
    if (id < 0) {
        const auto &icfg = instrument.get_config();
        Side close_side;
        switch (id) {
            case overall_position:
                for (const Position &pos: ii.list) {
                    realize_position(icfg, pos, price);
                }
                ii.list.clear();
                return true;
            case buy_position: close_side = Side::buy;break;
            case sell_position: close_side = Side::sell;break;
            default: return false;
        };
        bool ok = false;
        ii.list.erase(std::remove_if(ii.list.begin(), ii.list.end(), [&](const Position &pos){
            if (pos.side != close_side) return false;
            realize_position(icfg, pos, price);
            ok = true;
            return true;
        }), ii.list.end());
        return ok;
    } else {
        auto iter = std::find_if(ii.list.begin(), ii.list.end(), [&](const Position &pos){
            return pos.id == id;
        });
        if (iter != ii.list.end()) {
            const auto &icfg = instrument.get_config();
            realize_position(icfg, *iter, price);
            ii.list.erase(iter);
            return true;
        } else {
            return false;
        }
    }

}

}
