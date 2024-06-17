#pragma once

#include "account.h"
#include <cmath>

namespace trading_api {



class IInstrument {
public:


    enum class Type {
        ///unknown type (not tradable)
        unknown,
        ///classic spot, no leverage, no short
        spot,
        ///futures/contract, can be leverage, can short
        contract,
        ///inverted COIN-M contract, strategy must invert price to calculate PNL
        inverted_contract,
        ///quantum contract, there is fixed ratio between PNL and profit in target currency
        quantum_contract,
        ///cfd market / however hedge is disabled by default
        cfd
    };

    struct Config {
        Type type = Type::unknown;
        ///quotation price step
        double tick_size = 1;
        ///amount step
        double lot_size = 1;
        ///multipler between contract size and real size (ex: multipler = 1000: amount=2.3 ~ 2300 shares)
        double lot_multiplier = 1;
        ///minimal allowed lot size
        double min_size = 0;
        ///minimal allowed volume (amount * multipler * price)
        double min_volume = 0;
        ///max allowed leverage
        double max_leverage = 0;
        ///fixed quantum factor between calculated pnl a real profit - (ex: 0.0001 USDT -> XBT = +10000 USDT profit = +1 XBT)
        double quantum_factor = 1;
        ///instrument is tradable (you can place orders)
        bool tradable = false;
        ///instrument can be shorted
        bool can_short = false;
    };


    constexpr virtual ~IInstrument() = default;

    virtual Account get_account() const = 0;

    virtual Config get_config() const = 0;
};


class NullInstrument: public IInstrument {
public:
    virtual Account get_account() const override {
        return {};
    }
    virtual IInstrument::Config get_config() const override {
        return {};
    }
    constexpr virtual ~NullInstrument() {}
};

class Instrument {
public:
    using Config = IInstrument::Config;
    using Type = IInstrument::Type;


    static constexpr NullInstrument null_instrument = {};
    static std::shared_ptr<const IInstrument> null_instrument_ptr;

    Instrument():_ptr(null_instrument_ptr) {}
    Instrument(std::shared_ptr<const IInstrument> x): _ptr(std::move(x)) {}

    bool operator==(const Instrument &other) const = default;
    std::strong_ordering operator<=>(const Instrument &other) const = default;


    ///Retrieve associated account - note the account state probably need to be updated
    Account get_account() const {return _ptr->get_account();}

    Config get_config() const {return _ptr->get_config();}

    struct Hasher {
        auto operator()(const Instrument &ord) const {
            std::hash<std::shared_ptr<const IInstrument> > hasher;
            return hasher(ord._ptr);
        }
    };



protected:
    std::shared_ptr<const IInstrument> _ptr;

};

inline std::shared_ptr<const IInstrument> Instrument::null_instrument_ptr = {&Instrument::null_instrument, [](auto){}};


///calculate pnl by type of instrument
/**
 * @param cfg instrument config
 * @param pos position
 * @param close_price closing price
 * @return pnl in real money
 */
inline double calc_pnl(const Instrument::Config &cfg, const Account::Position &pos, double close_price) {
    switch (cfg.type) {
        default: return pos.amount * static_cast<int>(pos.side) * cfg.lot_multiplier * (close_price - pos.open_price);
        case Instrument::Type::inverted_contract:
            return pos.amount * static_cast<int>(pos.side) * cfg.lot_multiplier * (1.0/pos.open_price - 1.0/close_price);
        case Instrument::Type::quantum_contract:
            return pos.amount * static_cast<int>(pos.side) * cfg.lot_multiplier * cfg.quantum_factor * (close_price - pos.open_price);
    }
}

///adjust price to nearest tick - avoid zero price
/**
 * @param cfg instrument config
 * @param price price
 * @return adjusted price
 */
inline double adjust_price(const Instrument::Config &cfg, double price) {
    return std::max(std::round(price/cfg.tick_size)*cfg.tick_size, cfg.tick_size);
}
///adjust amount to nearest lot
/**
 * @param cfg instrument config
 * @param amount amount
 * @return adjusted amount
 */
inline double adjust_amount(const Instrument::Config &cfg, double amount) {
    return std::round(amount/cfg.lot_size)*cfg.lot_size;
}
///convert amount to lot
inline double convert_amount_to_lot(const Instrument::Config &cfg, double amount) {
    return std::round(amount/(cfg.lot_multiplier*cfg.lot_size))*cfg.lot_size;
}
///convert lot to amount
inline double convert_lot_to_amount(const Instrument::Config &cfg, double lot) {
    return lot * cfg.lot_multiplier;
}
///calc min amount for given price
inline double calc_min_amount(const Instrument::Config &cfg, double price) {
    double from_volume =  std::ceil(cfg.min_volume / (price * cfg.lot_multiplier * cfg.lot_size))*cfg.lot_multiplier;
    return std::max({cfg.lot_size, cfg.min_size, from_volume});
}

}

