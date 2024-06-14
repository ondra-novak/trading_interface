#pragma once

#include "abstract_account.h"

namespace trading_ifc {



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
        double tick_size = 1;
        double lot_size = 1;
        double lot_multiplier = 1;
        double min_size = 0;
        double min_volume = 0;
        double max_leverage = 0;
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


}

