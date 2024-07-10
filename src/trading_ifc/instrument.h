#pragma once

#include <cmath>
#include "position.h"

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
        ///fixed quantum factor between calculated pnl a real profit - (ex: 0.0001 USDT -> XBT = +10000 USDT profit = +1 XBT)
        double quantum_factor = 1;
        ///instrument is tradable (you can place orders)
        bool tradable = false;
        ///instrument can be shorted
        bool can_short = false;
    };


    constexpr virtual ~IInstrument() = default;

    ///retrieve instrument account
    /**
     * @return configuration is returned as reference to improve performance
     * in many places where configuration is used. The instrument must
     * allocate configuration inside its instance
     */
    virtual const Config &get_config() const = 0;

    ///Retrieve internal instrument ID
    virtual std::string get_id() const = 0;

    ///Retrieve instrument's label (user defined)
    virtual std::string get_label() const = 0;

};


class NullInstrument: public IInstrument {
public:

    static constexpr Config null_config = {};

    virtual const IInstrument::Config &get_config() const override {
        return null_config;
    }
    virtual std::string get_id() const override {return {};}
    virtual std::string get_label() const override {return {};}

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


    const Config &get_config() const {return _ptr->get_config();}

    struct Hasher {
        auto operator()(const Instrument &ord) const {
            std::hash<std::shared_ptr<const IInstrument> > hasher;
            return hasher(ord._ptr);
        }
    };

    ///Retrieve internal instrument ID
    /**Instrument internal ID is used to record instrument refernece
     * on the fill. You can use Instrument instance to retrieve fills
     * on given instrument. However, you cannot convert ID to Instrument.
     * @return
     */
    std::string get_id() const {
        return _ptr->get_id();
    }

    ///Retrieve label
    /**
     * The label is identificator provided by the user. It can specify
     * purpose of this instrument in context of the strategy
     * (for example "main", "hedge", etc)
     *
     * @return label
     */
    std::string get_label() const {
        return _ptr->get_label();
    }


    auto get_handle() const {return _ptr;}


    ///converts lot to amount
    /**
     * Instrument can present amount of shares in lots. This function converts
     * lots to real amount.
     *
     * Example: 1 lot is 100 shares. So reported position is 1.23 ~ amount = 123 shares
     *
     * @param lots reported lots
     * @return real amount
     */

    double lot_to_amount(double lots) const {
        return lot_to_amount(get_config(), lots);
    }

    ///converts real amount to lots
    /**
     * Instrument can present amount of shares in lots. This function converts
     * real amount to lots
     *
     * Example: 1 lot is 100 shares. You want to buy 123 shares, you need to buy
     * 1.23 lots
     *
     * @param amount real amount
     * @return lots
     */
    double amount_to_lot(double amount) const{
        return amount_to_lot(get_config(), amount);
    }

    ///converts quotation price to real price
    /**
     * quotation price can be different than real price. The real price can
     * be used to calculate profit or loss in equation
     *
     * real_amount * (real_close - real_open)
     *
     * This function return real price for quotation price
     *
     * Example: quantum contract defines a quantu 0.001. The quotation for the
     * contract is 147321. Real price for the contract is 147.321
     *
     * @param price quotation price
     * @return real price
     */
    double quotation_to_price(double price) const {
        return quotation_to_price(get_config(), price);
    }
    ///convert real price to quotation price
    /**
     * quotation price can be different than real price. The real price can
     * be used to calculate profit or loss in equation
     *
     * real_amount * (real_close - real_open)
     *
     * This function return quotation price for a real price
     *
     * Example: quantum contract defines a quantu 0.001. The real price of
     * this contract is 147.321. Quotation price is 147321
     *
     * @param price real price
     * @return quotation price
     */
    double price_to_quotation(double price) const {
        return price_to_quotation(get_config(), price);
    }

    ///calculates pnl
    /**
     *
     * @param pos position reported from account (which is always in instrument
     * realm)
     * @param close_price close position (quotation price)
     * @return pnl
     */
    double calculate_pnl(const Position &pos, double close_price) const {
        return calculate_pnl(get_config(), pos, close_price);
    }
    ///adjust price to nearest tick
    /**
     * @param price price
     * @return adjusted price
     */
    double adjust_price(double price) const {
        return adjust_price(get_config(), price);
    }
    ///adjust amount to nearest lot
    /**
     * @param amount amount in lots
     * @return adjusted amount in lots
     */
    double adjust_lot(double amount) const {
        return adjust_lot(get_config(), amount);
    }
    double adjust_lot_down(double amount) const {
        return adjust_lot_down(get_config(), amount);
    }
    ///calculate minimal real amount for given price
    /**
     * @param price quotation price
     * @return minimal real amount (to_real_position)
     */
    double calc_min_amount(double price) const {
        return calc_min_amount(get_config(), price);
    }

    double adjust_amount(double price, double size, bool size_is_volume) const {
        return adjust_amount(get_config(), price, size, size_is_volume);
    }
    static double lot_to_amount(const Config &cfg, double lots) {
        if (cfg.type == Type::inverted_contract) {
             lots = -lots;
        }
        lots *= cfg.lot_multiplier;
        return lots;
    }
    static double amount_to_lot(const Config &cfg, double amount) {
        amount/=cfg.lot_multiplier;
        if (cfg.type == Type::inverted_contract) {
             amount = -amount;
        }
        return amount;
    }
    static double quotation_to_price(const Config &cfg, double price) {
        switch (cfg.type){
            case Type::inverted_contract: return 1.0/price;
            case Type::quantum_contract: return price * cfg.quantum_factor;
            default: return price;
        }
    }
    static double price_to_quotation(const Config &cfg, double price) {
        switch (cfg.type){
            case Type::inverted_contract: return 1.0/price;
            case Type::quantum_contract: return price / cfg.quantum_factor;
            default: return price;
        }
    }
    static double calculate_pnl(const Config &cfg, const Position &pos, double close_price) {
        double amount = pos.amount * static_cast<int>(pos.side) * cfg.lot_multiplier;
        switch (cfg.type) {
            case Type::inverted_contract:
                return amount * (1.0/pos.open_price - 1.0/close_price);
            case Type::quantum_contract:
                return amount * cfg.quantum_factor * (close_price - pos.open_price);
            default:
                return amount * (close_price - pos.open_price);
        }
    }
    static double adjust_price(const Instrument::Config &cfg, double price) {
        return std::max(std::round(price/cfg.tick_size)*cfg.tick_size, cfg.tick_size);
    }
    static double adjust_lot(const Config &cfg, double amount)  {
        return std::round(amount/cfg.lot_size)*cfg.lot_size;
    }
    static double adjust_lot_down(const Config &cfg, double amount)  {
        return std::floor(amount/cfg.lot_size)*cfg.lot_size;
    }
    static double adjust_lot_up(const Config &cfg, double amount)  {
        return std::ceil(amount/cfg.lot_size)*cfg.lot_size;
    }
    static double calc_min_amount(const Config &cfg, double price) {
        double real_min_size = std::abs(lot_to_amount(cfg, cfg.min_size));
        double real_lot_size = std::abs(lot_to_amount(cfg, cfg.lot_size));
        double real_min_vol = std::abs(cfg.min_volume/ quotation_to_price(cfg, price));
        return std::max(std::max(real_min_size, real_lot_size), real_min_vol);
    }
    static double calc_margin(const Config &cfg, double price, double amount, double leverage) {
        double real_amount = lot_to_amount(cfg, amount);
        double real_price = quotation_to_price(cfg, price);
        return real_amount * real_price / leverage;
    }

    static double adjust_amount(const Instrument::Config &cfg, double price, double size, bool size_is_volume) {
        if (size_is_volume) {
            double ms = calc_min_amount(cfg, price);
            double lt = adjust_lot_down(cfg,amount_to_lot(cfg, size/quotation_to_price(cfg, price)));
            if (ms > lot_to_amount(cfg, lt)) return 0;
            return lt;
        } else {
            double ms = adjust_lot_up(cfg,amount_to_lot(cfg, calc_min_amount(cfg, price)));
            return std::max(ms,adjust_lot(cfg, size));
        }
    }


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
inline double calculate_pnl(const Instrument::Config &cfg, const Position &pos, double close_price) {
    switch (cfg.type) {
        default: return pos.amount * static_cast<int>(pos.side) * cfg.lot_multiplier * (close_price - pos.open_price);
        case Instrument::Type::inverted_contract:
            return pos.amount * static_cast<int>(pos.side) * cfg.lot_multiplier * (1.0/pos.open_price - 1.0/close_price);
        case Instrument::Type::quantum_contract:
            return pos.amount * static_cast<int>(pos.side) * cfg.lot_multiplier * cfg.quantum_factor * (close_price - pos.open_price);
    }
}

inline double price_instrument_to_strategy(const Instrument::Config &cfg, double price) {
    switch (cfg.type) {
        default: return price;
        case Instrument::Type::inverted_contract: return 1.0/price;
    }
}

}

