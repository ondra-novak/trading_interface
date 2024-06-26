#include "simulator_account.h"

#include <algorithm>
#include "../common/acb.h"

namespace trading_api {

SimulAccount::SimulAccount(std::string label, std::string currency, double equity, double leverage)
        :_label(label), _currency(currency), _equity(equity), _leverage(leverage)
{

}

void SimulAccount::record_fill(const Instrument &instrument, Side side, double price,
        double amount, Order::Behavior behaviour) {

    std::lock_guard _(_mx);
    auto icfg = instrument.get_config();


    PositionList &lst = _positions[instrument];
    if (behaviour != Order::Behavior::hedge) {
        Side close_side = reverse(side);
        lst.erase(std::remove_if(lst.begin(), lst.end(), [&](const Position &pos){
            if (pos.side == close_side && pos.amount <= amount+pos.amount*1e-10) {
                realize_position(icfg, pos, price);
                amount -= pos.amount;
                return true;
            }
            return false;
        }), lst.end());
        if (amount > 0) {
            auto iter = std::find_if(lst.begin(), lst.end(), [&](const Position &pos){
                return pos.side == close_side;
            });
            if (iter != lst.end()) {
                Position p = *iter;
                iter->id = _position_counter++;
                iter->amount -= amount;
                realize_position(icfg, p, price);
                return;
            }
        }
        if (behaviour == Order::Behavior::reduce) {
            return;
        }
    }
    lst.push_back({
        _position_counter++,
        side,
        amount,
        price,
    });

}

SimulAccount::PositionList SimulAccount::get_all_positions(const Instrument &instrument) const {
    std::lock_guard _(_mx);
    auto iter = _positions.find(instrument);
    if (iter == _positions.end()) return {};
    return iter->second;
}

SimulAccount::OverallPosition SimulAccount::get_position(const Instrument &instrument) const {
    OverallPosition out;
    out.id = overall_position;
    ACB acb;

    std::lock_guard _(_mx);
    auto iter = _positions.find(instrument);
    if (iter != _positions.end()) {
        for (const Position &pos: iter->second) {
            acb = acb(pos.open_price, pos.amount * static_cast<int>(pos.side));
        }
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
    auto iter = _positions.find(instrument);
    if (iter != _positions.end()) {
        for (const Position &pos: iter->second) {
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
            auto iter = _positions.find(instrument);
            if (iter != _positions.end()) {
                const PositionList &lst = iter->second;
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
    return {
        _equity,
        _equity,
        0,
        _leverage,
        _currency

    };
}


void SimulAccount::realize_position(const Instrument::Config &icfg, const Position &pos, double price) {

    _equity += calc_pnl(icfg, pos, price);

}

bool SimulAccount::close_position(const Instrument &instrument, PositionID id, double price) {
    std::lock_guard _(_mx);
    PositionList &lst = _positions[instrument];
    if (lst.empty()) return false;
    if (id < 0) {
        auto icfg = instrument.get_config();
        Side close_side;
        switch (id) {
            case overall_position:
                for (const Position &pos: lst) {
                    realize_position(icfg, pos, price);
                }
                lst.clear();
                return true;
            case buy_position: close_side = Side::buy;break;
            case sell_position: close_side = Side::sell;break;
            default: return false;
        };
        bool ok = false;
        lst.erase(std::remove_if(lst.begin(), lst.end(), [&](const Position &pos){
            if (pos.side != close_side) return false;
            realize_position(icfg, pos, price);
            ok = true;
            return true;
        }), lst.end());
        return ok;
    } else {
        auto iter = std::find_if(lst.begin(), lst.end(), [&](const Position &pos){
            return pos.id == id;
        });
        if (iter != lst.end()) {
            auto icfg = instrument.get_config();
            realize_position(icfg, *iter, price);
            lst.erase(iter);
            return true;
        } else {
            return false;
        }
    }

}

}
